#!/usr/bin/env bash
#
# run_equiv.sh ── end-to-end fuzz → PnR → equivalence flow
#
#
# External deps:  bash 4, cmake + make, Vivado, Yosys (+smtbmc)
# ---------------------------------------------------------------------------

set -euo pipefail

# ────────────────  USER-TUNABLE KNOBS (override via env) ──────────────────
TOP=${TOP:-top}

OUTDIR=${OUTDIR:-output}
LOG_DIR=${LOG_DIR:-"$OUTDIR/logs"}
PERMANENT_LOGS=${PERMANENT_LOGS:-"logs"}

LIBRARY=${LIBRARY:-hardware/cells/xilinx.yaml}
CONFIG=${CONFIG:-config/settings.toml}
PRIMS=${PRIMS:-"+/xilinx/cells_map.v +/xilinx/cells_sim.v"}

XILINX_TCL=${XILINX_TCL:-flows/vivado/impl.tcl}

RTL_NET=${RTL_NET:-"$OUTDIR/post_synth.v"}
PNR_NET=${PNR_NET:-"$OUTDIR/post_impl.v"}
FUZZ_NET="$OUTDIR/fuzzed_netlist.v"

SEED=${SEED:-$RANDOM}
DATE=$(date +%Y-%m-%d_%H-%M-%S)
# ───────────────────────────────────────────────────────────────────────────



# ────────────────  helpers  ────────────────────────────────────────────────
blue()  { printf "\033[0;34m[INFO] \033[0m%s\n"  "$*"; }
yellow(){ printf "\033[0;33m[WARN] \033[0m%s\n"  "$*"; }
green() { printf "\033[0;32m[PASS] \033[0m%s\n"  "$*"; }
red()   { printf "\033[0;31m[FAIL] \033[0m%s\n"  "$*"; }

log_failed_seed() {
    mkdir -p "$PERMANENT_LOGS"
    shopt -s nullglob
    cp "$LOG_DIR/${DATE}"_*.{log,jou} "$PERMANENT_LOGS/" 2>/dev/null || true
    cp "$CONFIG" "$PERMANENT_LOGS/config_${DATE}.toml"
    printf "%-20s | SEED: %-10s | MESSAGE: %s\n" "$DATE" "$SEED" "$*" >> "$PERMANENT_LOGS/failed_seeds.log"
    red "Seed $SEED captured - detailed logs in $PERMANENT_LOGS"
    shopt -u nullglob
}

die() { red "$*"; exit 1; }

trap 'red "Failure in command: $BASH_COMMAND"; log_failed_seed "Failure in command: $BASH_COMMAND"; exit 1' ERR

tmpl() {
    local src="$1"
    local dst
    dst="$OUTDIR/$(basename "$src" .in)"
    sed -e "s|__RTL__|$RTL_NET|g"  \
        -e "s|__PNR__|$PNR_NET|g"  \
        -e "s|__TOP__|$TOP|g"      \
        -e "s|__PRIMS__|$PRIMS|g"  \
        -e "s|__OUT__|$OUTDIR|g"   \
        "$src" > "$dst"
    echo "$dst"
}

# ───────────────────────────────────────────────────────────────────────────

mkdir -p "$OUTDIR" "$LOG_DIR"

blue "┌───────────────────── run_equiv ─────────────────────"
blue "│ RTL  : $RTL_NET"
blue "│ PNR  : $PNR_NET"
blue "│ PRIMS: $PRIMS"
blue "│ SEED : $SEED"
blue "└──────────────────────────────────────────────────────"

# ── build & run fuznet ────────────────────────────────────────────
./scripts/build.sh  >"$LOG_DIR/${DATE}_build.log" 2>&1
./build/fuznet -l "$LIBRARY" \
               -c "$CONFIG"  \
               -s "$SEED"    \
               -o "${FUZZ_NET%.v}"      \
               >"$LOG_DIR/${DATE}_fuznet.log" 2>&1 \
               || die "fuznet failed"
blue "fuznet finished"

# ── Vivado PnR ────────────────────────────────────────────────────
blue "Running Vivado PnR"
vivado -mode batch \
       -log "$LOG_DIR/${DATE}_vivado.log" \
       -journal "$LOG_DIR/${DATE}_vivado.jou" \
       -source "$XILINX_TCL" \
       -tclargs "$RTL_NET" "$PNR_NET" "$TOP" "$FUZZ_NET" \
       >/dev/null 2>&1
VIVADO_RET=$?

if [[ $VIVADO_RET -gt 128 ]]; then
    log_failed_seed "Vivado crashed with signal (ret=$VIVADO_RET)"
    die "Vivado Crashed (ret=$VIVADO_RET)"
elif [[ $VIVADO_RET -gt 0 ]]; then
    die "Vivado failed (ret=$VIVADO_RET)"
fi

rm -rf .Xil clockInfo.txt || true
blue "Vivado PnR finished"

# ── Structural equivalence ───────────────────────────────────────
blue "Structural equivalence check"
TMP_YS=$(tmpl flows/yosys/struct_check.ys.in)
if yosys -q -l "$LOG_DIR/${DATE}_struct.log" -s "$TMP_YS" >/dev/null 2>&1; then
    green "structural equivalence OK"
    exit 0
else
    yellow "structural check failed → proceeding to miter/BMC"
fi

# ── Miter equivalence ────────────────────────────────────────────
TMP_YS=$(tmpl flows/yosys/miter_check.ys.in)
MITER_LOG="$LOG_DIR/${DATE}_miter.log"
if yosys -q -l "$MITER_LOG" -s "$TMP_YS" >/dev/null 2>&1; then
    MITER_RET=0
else
    MITER_RET=$?
fi
MITER_TOKEN=$(grep -oE 'SUCCESS!|FAIL!|TIMEOUT!' "$MITER_LOG" || echo "UNKNOWN")

case "$MITER_TOKEN:$MITER_RET" in
    "SUCCESS!:0") green "miter check passed"; exit 0 ;;
    "TIMEOUT!:0") yellow "miter check timed out" ;;
    "FAIL!:1")    log_failed_seed "miter check failed"; die "miter check failed (ret=$MITER_RET token=$MITER_TOKEN), check $MITER_LOG" ;;
    *)            die    "miter unknown state (ret=$MITER_RET token=$MITER_TOKEN), check $MITER_LOG" ;;
esac

# ── BMC / induction ──────────────────────────────────────────────
blue "BMC (Z3, 1000 steps)"
if  yosys-smtbmc -s z3 -t 1000 \
                  --dump-vcd "$OUTDIR/bmc.vcd" \
                  "$OUTDIR/vivado.smt2" \
                  >"$LOG_DIR/${DATE}_bmc.log" 2>&1; then
    log_failed_seed "bmc failed"; die "BMC failed - check $LOG_DIR/${DATE}_bmc.log"
fi
green "BMC passed"

# ── 6. Induction proof ──────────────────────────────────────────────────
blue  "Induction (Z3, k<=128)"
if yosys-smtbmc -s z3 -i -t 128 \
                --dump-vcd "$OUTDIR/induct.vcd" \
                "$OUTDIR/vivado.smt2" \
                >"$LOG_DIR/${DATE}_induct.log" 2>&1; then
    green "Induction passed - equivalence proven"
    exit 0
else
    yellow "Induction failed - see $LOG_DIR/${DATE}_induct.log"
    yellow "No equivalence proven, but no counterexample found"
    exit 2
fi
