#!/usr/bin/env bash
#
# run_equiv.sh ── end-to-end fuzz → PnR → equivalence flow
#
# External deps: bash 4, cmake + make, Vivado, Yosys (+smtbmc)
# ---------------------------------------------------------------------------

set -euo pipefail

# ─────────────────────── USER-TUNABLE KNOBS (override via env) ──────────────
TOP=${TOP:-top}

WORKER_ID=${WORKER_ID:-0}

SEED=${SEED:-$(od -An -N4 -tu4 < /dev/urandom)}

EPOCH_START=$(date +%s)
DATE_TIME=$(date -d "@$EPOCH_START" +%Y-%m-%d_%H-%M-%S)
SEED_HEX=$(printf "0x%08x" "$SEED")

OUT_DIR=${OUT_DIR:-"tmp"}
LOG_DIR="$OUT_DIR/logs"

PERMANENT_LOGS=${PERMANENT_LOGS:-"logs"}

CELL_LIBRARY=${CELL_LIBRARY:-hardware/xilinx/cells.yaml}
SETTINGS_TOML=${SETTINGS_TOML:-config/settings.toml}
PRIMITIVES_V=${PRIMITIVES_V:-hardware/xilinx/cell_sim.v}

VIVADO_TCL=${VIVADO_TCL:-flows/vivado/impl.tcl}
VIVADO_BIN=${VIVADO_BIN:-/opt/Xilinx/Vivado/2024.2/bin/vivado}

FUZNET_BIN=${FUZNET_BIN:-fuznet}

SYNTH_TOP=${SYNTH_TOP:-synth}   # RTL (golden) hierarchy root
NETLIST_TOP=${NETLIST_TOP:-impl} # PnR (gate-level) hierarchy root

FUZZED_NETLIST_V="$OUT_DIR/fuzzed_netlist.v"

PORT_SPEC_JSON=${PORT_SPEC_JSON:-port_spec.json}

USE_SMTBMC=${USE_SMTBMC:-0}
# ───────────────────────────────────────────────────────────────────────────

# ───────────────────────────── helpers ─────────────────────────────────────
info()  { printf "\033[0;34m[INFO] \033[0m%s\n"  "$*"; }
warn()  { printf "\033[0;33m[WARN] \033[0m%s\n"  "$*"; }
pass()  { printf "\033[0;32m[PASS] \033[0m%s\n"  "$*"; }
fail()  { printf "\033[0;31m[FAIL] \033[0m%s\n"  "$*"; }

abort() {
    fail "$*"
    capture_failed_seed "$*"
    exit 1
}

capture_failed_seed() {
    # local save_dir="$PERMANENT_LOGS/${DATE_TIME}-${SEED_HEX}-w${WORKER_ID}"
    # mkdir -p "$save_dir"
    # cp -r "$OUT_DIR"/* "$save_dir/" 2>/dev/null || true
    # printf "%-20s | SEED: %-10s | MESSAGE: %s\n" "$DATE_TIME" "$SEED_HEX" "$*" >> "$PERMANENT_LOGS/failed_seeds.log"
    # fail "Seed $SEED_HEX captured - detailed logs in $PERMANENT_LOGS"
    echo "Seed $SEED_HEX failed"
}

# Create a temporary yosys script from a template, substituting knobs
#   $1 – template path (*.in)
mk_template() {
    local src="$1"
    local dst="$OUT_DIR/$(basename "$src" .in)"
    sed -e "s|__SYNTH__|$SYNTH_TOP|g"   \
        -e "s|__IMPL__|$NETLIST_TOP|g" \
        -e "s|__TOP__|$TOP|g"          \
        -e "s|__PRIMS__|$PRIMITIVES_V|g" \
        -e "s|__OUT__|$OUT_DIR|g"      \
        -e "s|__JSON__|$PORT_SPEC_JSON|g" \
        "$src" > "$dst"
    echo "$dst"
}

# ─────────────────────────── cleanup / stats ───────────────────────────────
RESULT_CATEGORY=""

on_exit() {
    local end_time=$(date +%s)
    local runtime=$(( end_time - EPOCH_START ))
    local human_date=$(date -d "@$EPOCH_START" '+%Y-%m-%d %H:%M:%S')

    local stats_json="${FUZZED_NETLIST_V%.v}_stats.json"

    mkdir -p "$PERMANENT_LOGS"
    local results_csv="$PERMANENT_LOGS/results.csv"
    if [[ ! -f $results_csv ]]; then
        echo "timestamp,worker,seed,category,runtime,input_nets,output_nets,total_nets,comb_modules,seq_modules,total_modules" \
             > "$results_csv"
    fi

    local in_nets= output_nets= total_nets=
    local comb_mods= seq_mods= total_mods=

    if [[ -f $stats_json ]]; then
        read -r in_nets output_nets total_nets \
                        comb_mods seq_mods total_mods < <(
        jq -r '.netlist_stats | [.input_nets,.output_nets,.total_nets,.comb_modules,.seq_modules,.total_modules] | @tsv' \
            "$stats_json"
        )
    else
        in_nets=NA output_nets=NA total_nets=NA
        comb_mods=NA seq_mods=NA total_mods=NA
    fi

    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
           "$human_date" "$WORKER_ID" "$SEED_HEX" "${RESULT_CATEGORY:-unknown}" "$runtime" \
           "$in_nets" "$output_nets" "$total_nets" \
           "$comb_mods" "$seq_mods" "$total_mods" >> "$results_csv"

    rm -rf "$OUT_DIR"
}
# trap 'on_exit' EXIT SIGINT SIGTERM

# Exit if any command in a pipeline fails & record the failing command
# trap 'fail "Failure in command: $BASH_COMMAND"; capture_failed_seed "Failure in command: $BASH_COMMAND"; RESULT_CATEGORY="${RESULT_CATEGORY:-error}"; exit 1' ERR

# ────────────────────────────── bootstrap ──────────────────────────────────
mkdir -p "$OUT_DIR" "$LOG_DIR"

cp "$CELL_LIBRARY" "$SETTINGS_TOML" "$VIVADO_TCL" "$OUT_DIR/"

CELL_LIBRARY_CP="$OUT_DIR/$(basename "$CELL_LIBRARY")"
SETTINGS_TOML_CP="$OUT_DIR/$(basename "$SETTINGS_TOML")"
VIVADO_TCL_CP="$OUT_DIR/$(basename "$VIVADO_TCL")"

info "┌────────────────────── run_equiv ──────────────────────"
info "│ OUT_DIR : $OUT_DIR"
info "│ WORKER  : $WORKER_ID"
info "│ PRIMS   : $PRIMITIVES_V"
info "│ SEED    : $SEED_HEX"
info "└────────────────────────────────────────────────────────"

# ─────────── 1. Fuzz netlist generation ───────────────────────────────────
"$FUZNET_BIN"  -l "$CELL_LIBRARY_CP"        \
               -c "$SETTINGS_TOML_CP"       \
               -s "$SEED"                   \
               -v                           \
               -j                           \
               -o "${FUZZED_NETLIST_V%.v}"  \
               >"$LOG_DIR/fuznet.log" 2>&1  || { RESULT_CATEGORY="fuznet_fail"; abort "fuznet failed"; }
info "fuznet finished"

# ─────────── 2. Vivado implementation ─────────────────────────────────────
info "Running Vivado PnR"
VIVADO_RET=0
"$VIVADO_BIN" -mode batch                    \
              -log "$LOG_DIR/vivado.log"     \
              -journal "$LOG_DIR/vivado.jou" \
              -source "$VIVADO_TCL_CP"       \
              -tclargs "$OUT_DIR" "$SYNTH_TOP" "$NETLIST_TOP" "$TOP" "$FUZZED_NETLIST_V" \
              >/dev/null 2>&1 || VIVADO_RET=$?

if (( VIVADO_RET > 128 )); then
    RESULT_CATEGORY="vivado_crash"
    capture_failed_seed "Vivado crashed (ret=${VIVADO_RET})"
    fail "Vivado crashed"
    exit 0
elif (( VIVADO_RET > 0 )); then
    RESULT_CATEGORY="vivado_fail"
    abort "Vivado failed (ret=${VIVADO_RET})"
fi
rm -f clockInfo.txt || true
info "Vivado PnR finished"

# ─────────── 3. Structural equivalence (yosys) ────────────────────────────
info "Structural equivalence check"
TMP_YS=$(mk_template flows/yosys/struct_check.ys.in)
if yosys -q -l "$LOG_DIR/struct.log" -s "$TMP_YS" >/dev/null 2>&1; then
    pass "structural equivalence OK"
    RESULT_CATEGORY="structural_pass"
    exit 0
else
    warn "structural check failed → falling back to functional checks"
fi

# ─────────── 4. Functional miter equivalence (yosys-sat) ──────────────────
info "Miter equivalence check"
TMP_YS=$(mk_template flows/yosys/miter_check.ys.in)
MITER_LOG="$LOG_DIR/miter.log"
MITER_RET=0
yosys -q -l "$MITER_LOG" -s "$TMP_YS" >/dev/null 2>&1 || MITER_RET=$?
MITER_TOKEN=$(grep -oE 'SUCCESS!|FAIL!|TIMEOUT!' "$MITER_LOG" || echo "UNKNOWN")

case "$MITER_TOKEN:$MITER_RET" in
    "SUCCESS!:0") pass  "miter check passed"      ; MITER_STATUS="pass"    ;;
    "TIMEOUT!:0") warn  "miter check timed out"   ; MITER_STATUS="timeout" ;;
    "FAIL!:1")    fail  "miter check failed"      ; MITER_STATUS="fail"    ;;
    *)            RESULT_CATEGORY="miter_unknown" ; abort "miter unknown state (ret=$MITER_RET token=$MITER_TOKEN)" ;;
esac

if [[ $MITER_STATUS == "pass" ]]; then
    RESULT_CATEGORY="miter_pass"
    exit 0
fi

# ─────────── 5. Verilator simulation fallback ─────────────────────────────
info "Verilator simulation"
VERILATOR_STATUS="unknown"
if ./scripts/gen_miter.py          \
        --outdir "$OUT_DIR"        \
        --seed "$SEED_HEX"         \
        --json "$PORT_SPEC_JSON"   \
        --gold-top "$SYNTH_TOP"    \
        --gate-top "$NETLIST_TOP"  \
        --tb "eq_top_tb.cpp"       \
        --cycles 100
then
    verilator -cc --exe --build  --trace     \
              -DGLBL -Wno-fatal -I"$OUT_DIR" \
              --trace-underscore             \
              -Mdir "$OUT_DIR/build"         \
              "$OUT_DIR/eq_top.v"            \
              "$PRIMITIVES_V"                \
              "$OUT_DIR/eq_top_tb.cpp"       \
              > "$LOG_DIR/verilator.log" 2>&1 || VERILATOR_STATUS="build_failed"
else
    fail "Failed to generate Verilator testbench"
fi

if [[ $VERILATOR_STATUS != "build_failed" ]]; then
    if "$OUT_DIR/build/Veq_top" >> "$LOG_DIR/verilator.log" 2>&1; then
        pass  "Verilator simulation passed"
        VERILATOR_STATUS="pass"
    else
        fail "Verilator simulation failed"
        VERILATOR_STATUS="fail"
    fi
fi

RESULT_CATEGORY="verilator_${VERILATOR_STATUS}_miter_${MITER_STATUS}"

case "${VERILATOR_STATUS}:${MITER_STATUS}" in
    "pass:timeout")         warn  "Verilator passed, but miter timed out"   ;;
    "pass:fail")            warn  "Verilator passed, but miter failed"      ; capture_failed_seed "Verilator passed, but miter failed"      ;;
    "build_failed:fail")    warn  "Verilator build failed, miter failed"    ; capture_failed_seed "Verilator build failed, miter failed"    ;;
    "build_failed:timeout") warn  "Verilator build failed, miter timed out" ; capture_failed_seed "Verilator build failed, miter timed out" ;;
    "fail:timeout")         warn  "Verilator failed, miter timed out"       ; capture_failed_seed "Verilator failed, miter timed out"       ;;
    "fail:fail")            fail  "Verilator failed, miter failed"          ; capture_failed_seed "Verilator failed, miter failed"          ;;
    *)                      abort "Verilator unknown state (verilator=$VERILATOR_STATUS miter=$MITER_STATUS)" ;;
esac

(( USE_SMTBMC == 0 )) && exit 0

# ─────────── 6. Bounded model checking (smtbmc) ───────────────────────────
info "BMC (Z3, 1000 steps, timeout 60s)"
BMC_LOG="$LOG_DIR/bmc.log"
if yosys-smtbmc -s z3 -t 1000 --timeout 60 --dump-vcd "$OUT_DIR/bmc.vcd" "$OUT_DIR/eq_top.smt2" \
               > "$BMC_LOG" 2>&1; then
    pass "BMC passed - equivalence proven"
else
    case $(grep -oE 'timeout|FAILED' "$BMC_LOG" || echo "UNKNOWN") in
        "timeout")  warn "BMC timed out" ;;
        "FAILED")   capture_failed_seed "BMC failed - counterexample found" ; RESULT_CATEGORY="bmc_fail" ; fail "BMC failed - counterexample found" ; exit 0 ;;
        *)        RESULT_CATEGORY="bmc_unknown" ; abort "BMC unknown outcome" ;;
    esac
fi

# ─────────── 7. Simple induction proof (k <= 128) ─────────────────────────
info "Induction (Z3, k<=128, timeout 60s)"
if yosys-smtbmc -s z3 -i -t 128 --timeout 60 --dump-vcd "$OUT_DIR/induct.vcd" "$OUT_DIR/eq_top.smt2" \
               > "$LOG_DIR/induct.log" 2>&1; then
    pass "Induction passed - equivalence proven"
    RESULT_CATEGORY="induction_pass"
else
    warn "Induction failed - see $LOG_DIR/induct.log"
    RESULT_CATEGORY="no_equivalence_proven"
fi
