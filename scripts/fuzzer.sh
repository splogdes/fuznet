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

SEED=${SEED:-$(od -An -N4 -tu4 < /dev/urandom)}
DATE=$(date +%Y-%m-%d_%H-%M-%S)

SEED_HEX=$(printf "0x%08x" "$SEED")

OUT_DIR=${OUT_DIR:-"tmp-$DATE-$SEED_HEX-w$WORKER_ID"}
LOG_DIR="$OUT_DIR/logs"

PERMANENT_LOGS=${PERMANENT_LOGS:-"logs"}

LIBRARY=${LIBRARY:-hardware/xilinx/cells.yaml}
CONFIG=${CONFIG:-config/settings.toml}
PRIMS=${PRIMS:-hardware/xilinx/cell_sim.v}

XILINX_TCL=${XILINX_TCL:-flows/vivado/impl.tcl}
VIVADO_BIN=${VIVADO_BIN:-"/opt/Xilinx/Vivado/2024.2/bin/vivado"}

FUZNET_BIN=${FUZNET_BIN:-"fuznet"}

RTL_TOP=${RTL_TOP:-"synth"}
PNR_TOP=${PNR_NET:-"impl"}
FUZZ_NET="$OUT_DIR/fuzzed_netlist.v"

USE_SMTBMC=${USE_SMTBMC:-0}
# ───────────────────────────────────────────────────────────────────────────


# ──────────────── Cleanup on exit ──────────────────────────────────────────

START_TIME=$(date +%s)
result_category=""

log_stats_on_exit() {
    local end_time=$(date +%s)
    local runtime=$(( end_time - START_TIME ))
    local timestamp=$(date "+%Y-%m-%d %H:%M:%S")

    local fuznet_stats="${FUZZ_NET%.v}_stats.json"

    mkdir -p "$PERMANENT_LOGS"
    local results_csv="$PERMANENT_LOGS/results.csv"
    
    if [ ! -f $results_csv ]; then
        echo "timestamp,worker,seed,category,runtime,input_nets,output_nets,total_nets,comb_modules,seq_modules,total_modules" \
             > "$PERMANENT_LOGS/results.csv"
    fi

    local input_nets="N/A" output_nets="N/A" total_nets="N/A"
    local comb_modules="N/A" seq_modules="N/A" total_modules="N/A"
    
    if [ -f "$fuznet_stats" ]; then
        input_nets=$(   jq -r '.netlist_stats.input_nets'      "$fuznet_stats" )
        output_nets=$(  jq -r '.netlist_stats.output_nets'     "$fuznet_stats" )
        total_nets=$(   jq -r '.netlist_stats.total_nets'      "$fuznet_stats" )
        comb_modules=$( jq -r '.netlist_stats.comb_modules'    "$fuznet_stats" )
        seq_modules=$(  jq -r '.netlist_stats.seq_modules'     "$fuznet_stats" )
        total_modules=$(jq -r '.netlist_stats.total_modules'   "$fuznet_stats" )
    fi
        
    printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
        "$timestamp" "$WORKER_ID" "$SEED_HEX" "${result_category:-unknown}" "$runtime" \
        "$input_nets" "$output_nets" "$total_nets" \
        "$comb_modules" "$seq_modules" "$total_modules" \
        >> "$results_csv"

    rm -rf $OUT_DIR
}

trap 'log_stats_on_exit' EXIT SIGINT SIGTERM 

# ────────────────  helpers  ────────────────────────────────────────────────
blue()  { printf "\033[0;34m[INFO] \033[0m%s\n"  "$*"; }
yellow(){ printf "\033[0;33m[WARN] \033[0m%s\n"  "$*"; }
green() { printf "\033[0;32m[PASS] \033[0m%s\n"  "$*"; }
red()   { printf "\033[0;31m[FAIL] \033[0m%s\n"  "$*"; }

log_failed_seed() {
    local save_dir="$PERMANENT_LOGS/$DATE-$SEED_HEX-w$WORKER_ID"
    mkdir -p "$save_dir"
    cp -r "$OUT_DIR"/* "$save_dir/" || true
    printf "%-20s | SEED: %-10s | MESSAGE: %s\n" "$DATE" "$SEED_HEX" "$*" >> "$PERMANENT_LOGS/failed_seeds.log"
    red "Seed $SEED_HEX captured - detailed logs in $PERMANENT_LOGS"
    echo "Seed $SEED_HEX failed"
}

die() { red "$*"; log_failed_seed "$*" ; exit 1; }

trap 'red "Failure in command: $BASH_COMMAND"; log_failed_seed "Failure in command: $BASH_COMMAND"; \
      result_category="${result_category:-error}"; exit 1' ERR

tmpl() {
    local src="$1"
    local dst
    dst="$OUT_DIR/$(basename "$src" .in)"
    sed -e "s|__SYNTH__|$RTL_TOP|g"  \
        -e "s|__IMPL__|$PNR_TOP|g"  \
        -e "s|__TOP__|$TOP|g"      \
        -e "s|__PRIMS__|$PRIMS|g"  \
        -e "s|__OUT__|$OUT_DIR|g"   \
        -e "s|__JSON__|$json_file|g" \
        "$src" > "$dst"
    echo "$dst"
}

json_file="port_spec.json"
miter_status="unknown"
verilator_status="unknown"

# ───────────────────────────────────────────────────────────────────────────

mkdir -p "$OUT_DIR" 
mkdir -p "$LOG_DIR"

cp "$LIBRARY" "$CONFIG" "$XILINX_TCL" "$OUT_DIR/"

LIBRARY_CP="$OUT_DIR/$(basename "$LIBRARY")"
CONFIG_CP="$OUT_DIR/$(basename "$CONFIG")"
XILINX_TCL_CP="$OUT_DIR/$(basename "$XILINX_TCL")"

blue "┌───────────────────── run_equiv ─────────────────────"
blue "│ OUT_DIR: $OUT_DIR"
blue "│ WORKER: $WORKER_ID"
blue "│ PRIMS: $PRIMS"
blue "│ SEED : $SEED_HEX"
blue "└─────────────────────────────────────────────────────"

# ── build & run fuznet ────────────────────────────────────────────
"$FUZNET_BIN"  -l "$LIBRARY_CP"             \
               -c "$CONFIG_CP"              \
               -s "$SEED"                   \
               -v                           \
               -j                           \
               -o "${FUZZ_NET%.v}"          \
               >"$LOG_DIR/fuznet.log" 2>&1  \
               || { result_category="fuznet_fail"; die "fuznet failed"; }
blue "fuznet finished"

# ── Vivado PnR ────────────────────────────────────────────────────
blue "Running Vivado PnR"
VIVADO_RET=0
"$VIVADO_BIN"  -mode batch                                                  \
               -log "$LOG_DIR/vivado.log"                                   \
               -journal "$LOG_DIR/vivado.jou"                               \
               -source "$XILINX_TCL_CP"                                     \
               -tclargs "$OUT_DIR" "$RTL_TOP" "$PNR_TOP" "$TOP" "$FUZZ_NET" \
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

rm clockInfo.txt > /dev/null 2>&1 || true
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
    "SUCCESS!:0") green  "miter check passed"     ; miter_status="pass"    ;;
    "TIMEOUT!:0") yellow "miter check timed out"  ; miter_status="timeout" ;;
    "FAIL!:1")    red    "miter check failed"     ; miter_status="fail"    ;;
    *)            result_category="miter_unknown" ; die "miter unknown state (ret=$MITER_RET token=$MITER_TOKEN)" ;;
esac

if [[ $miter_status == "pass" ]]; then
    result_category="miter_pass"
    exit 0
fi

# ── Verilator simulation ─────────────────────────────────────────
blue "Verilator simulation"

if ./scripts/gen_miter.py       \
        --outdir $OUT_DIR   \
        --seed $SEED_HEX     \
        --json $json_file    \
        --gold-top $RTL_TOP  \
        --gate-top $PNR_TOP  \
        --tb "eq_top_tb.cpp" \
        --no-vcd             \
        --cycles 1000000
then
    verilator -cc --exe --build --trace      \
              -DGLBL -Wno-fatal -I"$OUT_DIR" \
              --trace-underscore             \
              -Mdir $OUT_DIR/build           \
              "$OUT_DIR/eq_top.v"            \
              $PRIMS                         \
              "$OUT_DIR/eq_top_tb.cpp"       \
              > "$LOG_DIR/verilator.log" 2>&1 || verilator_status="build_failed"
else
    red "Failed to generate Verilator testbench"
    verilator_status="build_failed"
fi

if [[ $verilator_status != "build_failed" ]]; then
    if $OUT_DIR/build/Veq_top  >> "$LOG_DIR/verilator.log" 2>&1; then
        green  "Verilator simulation passed"
        verilator_status="pass"
    else
        red "Verilator simulation failed"
        verilator_status="fail"
    fi
fi

result_category=$(printf "verilator_%s_miter_%s" "$verilator_status" "$miter_status")

case "$verilator_status:$miter_status" in
    "pass:timeout")         yellow "Verilator simulation passed, but miter timed out" ;;
    "pass:fail")            red "Verilator simulation passed, but miter failed"         ; log_failed_seed "Verilator simulation passed, but miter failed"       ;;
    "build_failed:fail")    red "Verilator build failed, miter failed"                  ; log_failed_seed "Verilator build failed, miter failed"                ;;
    "build_failed:timeout") red "Verilator build failed, miter timed out"               ; log_failed_seed "Verilator build failed, but miter timed out"         ;;
    "fail:timeout")         red "Verilator simulation failed, miter timed out"          ; log_failed_seed "Verilator simulation failed, but miter timed out"    ;;
    "fail:fail")            red "Verilator simulation failed, miter failed"             ; log_failed_seed "Verilator simulation failed, and miter failed"       ;;
    *)                      die "Verilator unknown state (status=$verilator_status miter_status=$miter_status)" ;;
esac

if [[ $USE_SMTBMC -eq 0 ]]; then
    exit 0
fi

# ── BMC / induction ──────────────────────────────────────────────
blue "BMC (Z3, 1000 steps, timeout 300s)"
BMC_RET=0
BMC_LOG="$LOG_DIR/bmc.log"
yosys-smtbmc -s z3 -t 1000                       \
                   --timeout 60                  \
                   --dump-vcd "$OUT_DIR/bmc.vcd" \
                   "$OUT_DIR/eq_top.smt2"        \
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
if yosys-smtbmc -s z3 -i -t 128                  \
                --timeout 60                     \
                --dump-vcd "$OUT_DIR/induct.vcd" \
                "$OUT_DIR/eq_top.smt2"           \
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
