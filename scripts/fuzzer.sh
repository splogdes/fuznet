#!/usr/bin/env bash
#
# run_equiv.sh ── end-to-end fuzz → PnR → equivalence flow
#
# External deps: bash 4, cmake + make, Vivado, Yosys (+smtbmc)
# ---------------------------------------------------------------------------

set -euo pipefail

# ────────────────  USER-TUNABLE KNOBS (override via env) ──────────────────
TOP=${TOP:-top}

WORKER_ID=${WORKER_ID:-0}

SEED=${SEED:-$RANDOM}
DATE=$(date +%Y-%m-%d_%H-%M-%S)

OUTDIR=${OUTDIR:-"tmp-$DATE-s$SEED-w$WORKER_ID"}
LOG_DIR="$OUTDIR/logs"

PERMANENT_LOGS=${PERMANENT_LOGS:-"logs"}

LIBRARY=${LIBRARY:-hardware/cells/xilinx.yaml}
CONFIG=${CONFIG:-config/settings.toml}
PRIMS=${PRIMS:-"+/xilinx/cells_map.v +/xilinx/cells_sim.v"}

XILINX_TCL=${XILINX_TCL:-flows/vivado/impl.tcl}
VIVADO_BIN=${VIVADO_BIN:-"/opt/Xilinx/Vivado/2024.2/bin/vivado"}

FUZNET_BIN=${FUZNET_BIN:-"fuznet"}

RTL_NET=${RTL_NET:-"$OUTDIR/post_synth.v"}
PNR_NET=${PNR_NET:-"$OUTDIR/post_impl.v"}
FUZZ_NET="$OUTDIR/fuzzed_netlist.v"
# ───────────────────────────────────────────────────────────────────────────


# ──────────────── Cleanup on exit ──────────────────────────────────────────

START_TIME=$(date +%s)
result_category=""
trap 'END_TIME=$(date +%s); RUNTIME=$(( END_TIME - START_TIME )); TIMESTAMP=$(date "+%Y-%m-%d %H:%M:%S"); \
      if [ ! -f "$PERMANENT_LOGS/results.csv" ]; then echo "timestamp,worker,seed,category,runtime" >> "$PERMANENT_LOGS/results.csv"; fi; \
      echo "$TIMESTAMP,$SEED,$WORKER_ID,${result_category:-unknown},$RUNTIME" >> "$PERMANENT_LOGS/results.csv"; rm -r $OUTDIR' EXIT SIGINT SIGTERM 

# ────────────────  helpers  ────────────────────────────────────────────────
blue()  { printf "\033[0;34m[INFO] \033[0m%s\n"  "$*"; }
yellow(){ printf "\033[0;33m[WARN] \033[0m%s\n"  "$*"; }
green() { printf "\033[0;32m[PASS] \033[0m%s\n"  "$*"; }
red()   { printf "\033[0;31m[FAIL] \033[0m%s\n"  "$*"; }

log_failed_seed() {
    local save_dir="$PERMANENT_LOGS/$DATE-s$SEED-w$WORKER_ID"
    mkdir -p "$save_dir"
    cp -r "$OUTDIR"/* "$save_dir/" || true
    printf "%-20s | SEED: %-10s | MESSAGE: %s\n" "$DATE" "$SEED" "$*" >> "$PERMANENT_LOGS/failed_seeds.log"
    red "Seed $SEED captured - detailed logs in $PERMANENT_LOGS"
}

die() { red "$*"; log_failed_seed "$*" ; exit 1; }

trap 'red "Failure in command: $BASH_COMMAND"; log_failed_seed "Failure in command: $BASH_COMMAND"; \
      result_category="${result_category:-error}"; exit 1' ERR

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

mkdir -p "$OUTDIR" 
mkdir -p "$LOG_DIR"
mkdir -p "$PERMANENT_LOGS"

cp "$LIBRARY" "$CONFIG" "$XILINX_TCL" "$OUTDIR/"

LIBRARY_CP="$OUTDIR/$(basename "$LIBRARY")"
CONFIG_CP="$OUTDIR/$(basename "$CONFIG")"
XILINX_TCL_CP="$OUTDIR/$(basename "$XILINX_TCL")"

blue "┌───────────────────── run_equiv ─────────────────────"
blue "│ RTL  : $RTL_NET"
blue "│ PNR  : $PNR_NET"
blue "│ PRIMS: $PRIMS"
blue "│ SEED : $SEED"
blue "└──────────────────────────────────────────────────────"

# ── build & run fuznet ────────────────────────────────────────────
"$FUZNET_BIN"  -l "$LIBRARY_CP" \
               -c "$CONFIG_CP"  \
               -s "$SEED"    \
               -v            \
               -o "${FUZZ_NET%.v}"      \
               >"$LOG_DIR/fuznet.log" 2>&1 \
               || { result_category="fuznet_fail"; die "fuznet failed"; }
blue "fuznet finished"

# ── Vivado PnR ────────────────────────────────────────────────────
blue "Running Vivado PnR"
VIVADO_RET=0
"$VIVADO_BIN" -mode batch \
               -log "$LOG_DIR/vivado.log" \
               -journal "$LOG_DIR/vivado.jou" \
               -source "$XILINX_TCL_CP" \
               -tclargs "$RTL_NET" "$PNR_NET" "$TOP" "$FUZZ_NET" \
               > /dev/null 2>&1 || VIVADO_RET=$?

if [[ $VIVADO_RET -gt 128 ]]; then
    result_category="vivado_crash"
    log_failed_seed "Vivado crashed with signal (ret=$VIVADO_RET)"
    red "Vivado Crashed (ret=$VIVADO_RET)"
    exit 0
elif [[ $VIVADO_RET -gt 0 ]]; then
    result_category="vivado_fail"
    die "Vivado failed (ret=$VIVADO_RET)"
fi

rm -rf .Xil clockInfo.txt || true
blue "Vivado PnR finished"

# ── Structural equivalence ───────────────────────────────────────
blue "Structural equivalence check"
TMP_YS=$(tmpl flows/yosys/struct_check.ys.in)
if yosys -q -l "$LOG_DIR/struct.log" -s "$TMP_YS" >/dev/null 2>&1; then
    green "structural equivalence OK"
    result_category="structural_pass"
    exit 0
else
    yellow "structural check failed → proceeding to miter/BMC"
fi

# ── Miter equivalence ────────────────────────────────────────────
blue "Miter equivalence check"
TMP_YS=$(tmpl flows/yosys/miter_check.ys.in)
MITER_LOG="$LOG_DIR/miter.log"
MITER_RET=0
yosys -q -l "$MITER_LOG" -s "$TMP_YS" >/dev/null 2>&1 || MITER_RET=$?
MITER_TOKEN=$(grep -oE 'SUCCESS!|FAIL!|TIMEOUT!' "$MITER_LOG" || echo "UNKNOWN")

case "$MITER_TOKEN:$MITER_RET" in
    "SUCCESS!:0") green "miter check passed"; result_category="miter_pass"; exit 0 ;;
    "TIMEOUT!:0") yellow "miter check timed out" ;;
    "FAIL!:1")    log_failed_seed "miter check failed"; result_category="miter_fail"; red "miter check failed (ret=$MITER_RET token=$MITER_TOKEN), check $MITER_LOG"; exit 0 ;;
    *)            result_category="miter_unknown"; die "miter unknown state (ret=$MITER_RET token=$MITER_TOKEN)" ;;
esac

# ── Verilator simulation ─────────────────────────────────────────
blue "Verilator simulation"

export SEED OUTDIR=$(realpath "$OUTDIR") CYCLES=${CYCLES:-1000000}
./scripts/gen_tb.py || { result_category="tb_gen_fail"; die "tb_gen.py failed"; }

verilator -cc --exe --build -O2 \
          -Mdir $OUTDIR/build \
          "$OUTDIR/eq_top.v" \
          "$OUTDIR/eq_top_tb.cpp" \
          > "$LOG_DIR/verilator.log" 2>&1 || { result_category="verilator_fail"; die "Verilator failed"; }

if ! $OUTDIR/build/Veq_top > /dev/null 2>&1; then
    log_failed_seed "Verilator simulation failed"
    result_category="verilator_fail"
    red "Verilator simulation failed, check $LOG_DIR/verilator.log"
    exit 0
fi

green "Verilator simulation passed"

# ── BMC / induction ──────────────────────────────────────────────
blue "BMC (Z3, 1000 steps, timeout 300s)"
BMC_RET=0
BMC_LOG="$LOG_DIR/bmc.log"
yosys-smtbmc -s z3 -t 1000 \
                   --timeout 60 \
                   --dump-vcd "$OUTDIR/bmc.vcd" \
                   "$OUTDIR/eq_top.smt2" \
                   >"$BMC_LOG" 2>&1 || BMC_RET=$?
BMC_TOKEN=$(grep -oE 'timeout|PASSED|FAILED' "$BMC_LOG" || echo "UNKNOWN")

case "$BMC_TOKEN:$BMC_RET" in
    "PASSED:0") green "BMC passed - equivalence proven" ;;
    "timeout:1") yellow "BMC timed out" ;;
    "FAILED:1") log_failed_seed "BMC failed - counterexample found"; result_category="bmc_fail"; red "BMC failed - counterexample found (ret=$BMC_RET token=$BMC_TOKEN), check $BMC_LOG"; exit 0 ;;
    *) result_category="bmc_unknown"; die "BMC unknown state (ret=$BMC_RET token=$BMC_TOKEN)" ;;
esac

# ── 6. Induction proof ──────────────────────────────────────────────────
blue "Induction (Z3, k<=128, timeout 300s)"
if yosys-smtbmc -s z3 -i -t 128 \
                --timeout 60 \
                --dump-vcd "$OUTDIR/induct.vcd" \
                "$OUTDIR/eq_top.smt2" \
                >"$LOG_DIR/induct.log" 2>&1; then
    green "Induction passed - equivalence proven"
    result_category="induction_pass"
    exit 0
else
    yellow "Induction failed - see $LOG_DIR/induct.log"
    yellow "No equivalence proven, but no counterexample found"
    result_category="No_equivalence_proven"
    exit 0
fi
