#!/usr/bin/env bash
#
# fuzzer.sh ── end-to-end fuzz → PnR → equivalence flow
#
# External deps: Bash 4, Vivado, Yosys (+smtbmc), Verilator
#--------------------------------------------------------------------------

set -euo pipefail

# ───── shared helpers / stage functions ───────────────────────────────────
source "$(dirname "$0")/../flows/fuzzing/lib.sh"
source "$(dirname "$0")/../flows/fuzzing/10_gen.sh"
source "$(dirname "$0")/../flows/fuzzing/20_impl.sh"
source "$(dirname "$0")/../flows/fuzzing/30_struct.sh"
source "$(dirname "$0")/../flows/fuzzing/40_miter.sh"
source "$(dirname "$0")/../flows/fuzzing/50_verilator.sh"
source "$(dirname "$0")/../flows/fuzzing/70_reduction.sh"
# Optional SMT checks
source "$(dirname "$0")/../flows/fuzzing/60_bmc.sh"
source "$(dirname "$0")/../flows/fuzzing/61_induct.sh"

# ───── user-tunable knobs (env-override) ──────────────────────────────────
WORKER_ID=${WORKER_ID:-0}
SYNTH_TOP=${SYNTH_TOP:-synth}     # RTL   hierarchy root
IMPL_TOP=${IMPL_TOP:-impl}  # gate-level hierarchy root
FUZZED_TOP="fuzzed_netlist"       # basename (no .v)

USE_SMTBMC=${USE_SMTBMC:-0}       # 1 → run BMC + induction

# ───── directory scaffolding ──────────────────────────────────────────────
EPOCH_START=$(date +%s)
STAMP=$(date -d @"$EPOCH_START" +%Y-%m-%d_%H-%M-%S)
SEED_HEX=$(printf "0x%08x" "$SEED")

OUT_DIR=${OUT_DIR:-"tmp-${STAMP}-${SEED_HEX}-w${WORKER_ID}"}
LOG_DIR="$OUT_DIR/logs"
mkdir -p "$LOG_DIR"

PERMANENT_LOGS=${PERMANENT_LOGS:-logs}

# ───── result bookkeeping & traps ─────────────────────────────────────────
RESULT_CATEGORY=""

cp $CELL_LIB $VIVADO_TCL $SETTINGS_TOML "$OUT_DIR/" 2>/dev/null || true
export CELL_LIB="$OUT_DIR/$(basename "$CELL_LIB")"
export VIVADO_TCL="$OUT_DIR/$(basename "$VIVADO_TCL")"
export SETTINGS_TOML="$OUT_DIR/$(basename "$SETTINGS_TOML")"

on_exit() {
    local end_time=$(date +%s)
    local runtime=$(( end_time - EPOCH_START ))
    local human_date=$(date -d "@$EPOCH_START" '+%Y-%m-%d %H:%M:%S')

    local stats_json="$OUT_DIR/${FUZZED_TOP}_stats.json"

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

capture_failed_seed() {
    local msg=$1
    local save="$PERMANENT_LOGS/runs/${STAMP}-${SEED_HEX}-w${WORKER_ID}"
    mkdir -p "$save"
    cp -r "$OUT_DIR"/* "$save/" 2>/dev/null || true
    printf '%-19s | SEED: %-10s | %s\n' "$STAMP" "$SEED_HEX" "$msg" \
        >> "$PERMANENT_LOGS/failed_seeds.log"
    echo "SEED: $SEED_HEX | $msg" > "$save/seed.txt"
}

trap 'on_exit' EXIT
trap 'fail "error in $BASH_COMMAND"; RESULT_CATEGORY=driver_error; capture_failed_seed "$BASH_COMMAND"; exit 1' ERR
trap 'kill -- -$$' INT TERM

info "┌────────────────────── run_equiv ──────────────────────"
info "│ OUT_DIR : $OUT_DIR"
info "│ WORKER  : $WORKER_ID"
info "│ PRIMS   : $PRIMS_V"
info "│ SEED    : $SEED_HEX"
info "└───────────────────────────────────────────────────────"

# ───── fuzzed netlist generation ──────────────────────────────
if ! run_gen "$OUT_DIR" "$FUZZED_TOP" "$LOG_DIR"; then
    RESULT_CATEGORY="fuznet_fail"
    capture_failed_seed "fuznet failed"
    exit 1
fi

# ───── stage 20 – Vivado PnR ──────────────────────────────────
impl_ret=0
run_impl "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$FUZZED_TOP" "$LOG_DIR" || impl_ret=$?
case $impl_ret in
    0) ;;
    1) RESULT_CATEGORY="vivado_fail" ; exit 1 ;;
    2) RESULT_CATEGORY="vivado_crash"; exit 2 ;;
esac

# ───── structural equiv (Yosys) ───────────────────────────────
if run_struct "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR"; then
    RESULT_CATEGORY="structural_pass"
    exit 0
fi

# ───── SAT miter (Yosys-sat) ──────────────────────────────────
miter_ret=0
run_miter "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR" || miter_ret=$?

if (( miter_ret == 0 )); then
    RESULT_CATEGORY="miter_pass"
    exit 0
elif (( miter_ret == 2 )); then
    RESULT_CATEGORY="miter_unknown"
    capture_failed_seed "miter unknown state"
    exit 1
fi

# ───── Verilator simulation fallback ──────────────────────────
verilator_ret=0
run_verilator "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR" || verilator_ret=$?

if (( verilator_ret == 2 )); then
    RESULT_CATEGORY="verilator_error"
    capture_failed_seed "verilator error"
    exit 1
fi

# ───── optional SMTBMC (Z3) checks ────────────────────────────
if (( USE_SMTBMC )); then
    smt="$OUT_DIR/eq_top.smt2"
    if run_z3_smt "$OUT_DIR" "$smt" "$LOG_DIR"; then
        RESULT_CATEGORY="bmc_pass"
        exit 0
    else
        RESULT_CATEGORY="bmc_fail"
    fi

    if run_z3_induct "$OUT_DIR" "$smt" "$LOG_DIR"; then
        RESULT_CATEGORY="induct_pass"
        exit 0
    else
        RESULT_CATEGORY="induct_fail"
    fi
fi

# ───── result handling ────────────────────────────────────────
case "$miter_ret:$verilator_ret" in
    "1:1") RESULT_CATEGORY="miter_fail_verilator_fail"    ;;
    "1:0") RESULT_CATEGORY="miter_fail_verilator_pass"    ;   capture_failed_seed "miter failed, but Verilator passed"; exit 0 ;;
    "3:1") RESULT_CATEGORY="miter_timeout_verilator_fail" ;;
    "3:0") RESULT_CATEGORY="miter_timeout_verilator_pass" ;   exit 0 ;;
esac

# ───── Reduction of failed seeds ─────────────────────────────────

reduction_out_dir="$OUT_DIR/reduction"
reduction_log_dir="$reduction_out_dir/logs"

mkdir -p "$reduction_out_dir"
mkdir -p "$reduction_log_dir"

if ! run_reduction "$reduction_out_dir" "$OUT_DIR/$FUZZED_TOP.json" "$LOG_DIR/verilator_run.log" "$FUZZED_TOP" "$reduction_log_dir"; then
    capture_failed_seed "reduction failed"
    RESULT_CATEGORY="reduction_fail"
    exit 1
fi

# ───── Rerun Vivado on reduced netlist ─────────────────────────────
if ! run_impl "$reduction_out_dir" "$SYNTH_TOP" "$IMPL_TOP" "$FUZZED_TOP" "$reduction_log_dir"; then
    capture_failed_seed "reduced netlist Vivado failed"
    RESULT_CATEGORY="reduced_vivado_fail"
    exit 1
fi

# ───── Rerun Verilator simulation ──────────────────────────────────
verilator_ret=0
run_verilator "$reduction_out_dir" "$SYNTH_TOP" "$IMPL_TOP" "$reduction_log_dir" || verilator_ret=$?
if (( verilator_ret == 2 )); then
    capture_failed_seed "reduced netlist Verilator error"
    RESULT_CATEGORY="reduced_verilator_error"
    exit 1
fi

case "$miter_ret:$verilator_ret" in
    "1:1") RESULT_CATEGORY="miter_fail_verilator_fail_reduced"    ; capture_failed_seed "Miter failed, Verilator failed, reduction Success" ;;
    "1:0") RESULT_CATEGORY="miter_fail_verilator_fail_reduced"    ; capture_failed_seed "Miter failed, Verilator passed, reduction Failed"  ;;
    "3:1") RESULT_CATEGORY="miter_timeout_verilator_fail_reduced" ; capture_failed_seed "Miter timeout, Verilator failed, reduction Success" ;;
    "3:0") RESULT_CATEGORY="miter_timeout_verilator_pass_reduced" ; capture_failed_seed "Miter timeout, Verilator passed, reduction Failed"  ;;
esac

exit 0