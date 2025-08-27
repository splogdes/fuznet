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

EPOCH_START=$(date +%s%6N)
LAST_TIME=$(date +%s%6N)

STAMP=$(date -d @"${EPOCH_START:0:10}" +%Y-%m-%d_%H-%M-%S)
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
export HASH_FILE="${PERMANENT_LOGS}/seen_netlists.txt"

on_exit() {
    local end_time=$(date +%s%6N)
    local runtime=$(( end_time - EPOCH_START ))
    local human_date=$(date -d @"${EPOCH_START:0:10}" '+%Y-%m-%d %H:%M:%S')
    local stats_json="$OUT_DIR/${FUZZED_TOP}_stats.json"
    local results_csv="$PERMANENT_LOGS/results.csv"

    mkdir -p "$PERMANENT_LOGS"

    if [[ ! -f $results_csv ]]; then

        vivado_stats_header=$(
            ./scripts/vivado_log_parse.py "$LOG_DIR/vivado.log" --header-only
        )

        cat <<EOF > "$results_csv"
timestamp,worker,seed,category,runtime_micro,\
gen_micro,impl_micro,struct_micro,miter_micro,\
verilator_micro,z3_bmc_micro,z3_induct_micro,\
reduction_micro,impl_reduced_micro,verilator_reduced,\
input_nets,output_nets,total_nets,comb_modules,seq_modules,total_modules,\
input_nets_reduced,output_nets_reduced,total_nets_reduced,\
comb_modules_reduced,seq_modules_reduced,total_modules_reduced,\
max_iter,stop_iter_lambda,start_input_lambda,start_undriven_lambda,\
seq_mod_prob,seq_port_prob,AddRandomModule,AddExternalNet,\
AddUndriveNet,DriveUndrivenNet,DriveUndrivenNets,BufferUnconnectedOutputs,$vivado_stats_header
EOF

    fi

    declare -A STAGE_TIMES
    if [[ -f "$LOG_DIR/stage_runtimes.csv" ]]; then
        while IFS=, read -r stage time; do
            STAGE_TIMES["$stage"]=$time
        done < "$LOG_DIR/stage_runtimes.csv"
    fi

    local in_nets=NA output_nets=NA total_nets=NA
    local comb_mods=NA seq_mods=NA total_mods=NA
    local max_iter=NA stop_iter_lambda=NA start_input_lambda=NA start_undriven_lambda=NA
    local seq_mod_prob=NA seq_port_prob=NA
    local cmd_addmod=NA cmd_extnet=NA cmd_undrive=NA cmd_drive=NA cmd_drives=NA cmd_buf=NA

    if [[ -f $stats_json ]]; then
        read -r in_nets output_nets total_nets \
                comb_mods seq_mods total_mods < <(
            jq -r '.netlist_stats | [.input_nets,.output_nets,.total_nets,.comb_modules,.seq_modules,.total_modules] | @tsv' "$stats_json"
        )

        read -r max_iter stop_iter_lambda start_input_lambda start_undriven_lambda seq_mod_prob seq_port_prob < <(
            jq -r '.settings | [.max_iter, .stop_iter_lambda, .start_input_lambda, .start_undriven_lambda, .seq_mod_prob, .seq_port_prob] | @tsv' "$stats_json"
        )

        read -r cmd_addmod cmd_extnet cmd_undrive cmd_drive cmd_drives cmd_buf < <(
            jq -r '[.commands[] | .weight] | @tsv' "$stats_json"
        )
    fi

    local in_nets_reduced=NA output_nets_reduced=NA total_nets_reduced=NA
    local comb_mods_reduced=NA seq_mods_reduced=NA total_mods_reduced=NA

    if [[ -f "${reduction_out_dir:-none}/${FUZZED_TOP}_stats.json" ]]; then
        read -r in_nets_reduced output_nets_reduced total_nets_reduced \
                comb_mods_reduced seq_mods_reduced total_mods_reduced < <(
            jq -r '[.input_nets,.output_nets,.total_nets,.comb_modules,.seq_modules,.total_modules] | @tsv' \
                "$reduction_out_dir/${FUZZED_TOP}_stats.json"
        )
    fi


    result_line=$(printf "%s,%s,%s,%s,%s," \
        "$human_date" "$WORKER_ID" "$SEED_HEX" "${RESULT_CATEGORY:-unknown}" "$runtime")

    result_line+=$(printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s," \
        "${STAGE_TIMES[run_gen]:-NA}" "${STAGE_TIMES[run_impl]:-NA}" \
        "${STAGE_TIMES[run_struct]:-NA}" "${STAGE_TIMES[run_miter]:-NA}" \
        "${STAGE_TIMES[run_verilator]:-NA}" "${STAGE_TIMES[run_z3_smt]:-NA}" \
        "${STAGE_TIMES[run_z3_induct]:-NA}" "${STAGE_TIMES[run_reduction_reduced]:-NA}" \
        "${STAGE_TIMES[run_impl_reduced]:-NA}" "${STAGE_TIMES[run_verilator_reduced]:-NA}")

    result_line+=$(printf "%s,%s,%s,%s,%s,%s," \
        "$in_nets" "$output_nets" "$total_nets" "$comb_mods" "$seq_mods" "$total_mods")

    result_line+=$(printf "%s,%s,%s,%s,%s,%s," \
        "$in_nets_reduced" "$output_nets_reduced" "$total_nets_reduced" \
        "$comb_mods_reduced" "$seq_mods_reduced" "$total_mods_reduced")

    result_line+=$(printf "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s," \
        "$max_iter" "$stop_iter_lambda" "$start_input_lambda" "$start_undriven_lambda" \
        "$seq_mod_prob" "$seq_port_prob" \
        "$cmd_addmod" "$cmd_extnet" "$cmd_undrive" "$cmd_drive" "$cmd_drives" "$cmd_buf")

    result_line+=$(./scripts/vivado_log_parse.py "$LOG_DIR/vivado.log")

    echo "$result_line" >> "$results_csv"


    # rm -rf "$OUT_DIR" || true
}

time_stage() {
    if [[ ! -v reduction_out_dir ]]; then
        local stage_name=$(basename "$1")
    else
        local stage_name=$(basename "$1")"_reduced"
    fi
    local start_time=$(date +%s%6N)
    exit_code=0
    "$@" || exit_code=$?
    local end_time=$(date +%s%6N)
    local duration=$(( end_time - start_time ))
    echo "$stage_name,$duration" >> "$LOG_DIR/stage_runtimes.csv"
    return $exit_code
}

capture_failed_seed() {
    local msg=$1
    local dir=${2:-"common"}
    local save="$PERMANENT_LOGS/$dir/${STAMP}-${SEED_HEX}-w${WORKER_ID}"
    mkdir -p "$save"
    cp -r "$OUT_DIR"/* "$save/" 2>/dev/null || true
    printf '%-19s | SEED: %-10s | DIR: %-9s | %s\n' "$STAMP" "$SEED_HEX" "$dir" "$msg" \
        >> "$PERMANENT_LOGS/failed_seeds.log"
    echo "SEED: $SEED_HEX | $msg" > "$save/seed.txt"
    fail "$msg"
}

sigint_handler() {
    echo "Caught SIGINT or SIGTERM, exiting..."
    # rm -rf "$OUT_DIR" 2>/dev/null || true
    exit 1
}

trap 'on_exit' EXIT
trap 'fail "error in $BASH_COMMAND"; RESULT_CATEGORY=driver_error; capture_failed_seed "$BASH_COMMAND"; exit 1' ERR
trap 'sigint_handler' INT TERM

info "┌────────────────────── run_equiv ──────────────────────"
info "│ OUT_DIR : $OUT_DIR"
info "│ WORKER  : $WORKER_ID"
info "│ PRIMS   : $PRIMS_V"
info "│ SEED    : $SEED_HEX"
info "└───────────────────────────────────────────────────────"

# ───── fuzzed netlist generation ──────────────────────────────
if ! time_stage run_gen "$OUT_DIR" "$FUZZED_TOP" "$LOG_DIR"; then
    RESULT_CATEGORY="fuznet_fail"
    capture_failed_seed "fuznet failed"
    exit 1
fi
# ───── stage 20 – Vivado PnR ──────────────────────────────────
impl_ret=0
clk_period=10.000
time_stage run_impl "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$clk_period" "$FUZZED_TOP" "$LOG_DIR" || impl_ret=$?
case $impl_ret in
    0) ;;
    1) RESULT_CATEGORY="vivado_fail" ; exit 1 ;;
    2) RESULT_CATEGORY="vivado_crash"; capture_failed_seed "Vivado crashed" "rare"; exit 0 ;;
    3) RESULT_CATEGORY="vivado_timeout"; capture_failed_seed "Vivado timed out" "rare"; exit 1 ;;
esac

# ───── structural equiv (Yosys) ───────────────────────────────
if time_stage run_struct "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR"; then
    RESULT_CATEGORY="structural_pass"
    exit 0
fi

# ───── SAT miter (Yosys-sat) ──────────────────────────────────
miter_ret=0
time_stage run_miter "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR" || miter_ret=$?

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
time_stage run_verilator "$OUT_DIR" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR" || verilator_ret=$?

# ───── optional SMTBMC (Z3) checks ────────────────────────────
if (( USE_SMTBMC )); then
    smt="$OUT_DIR/eq_top.smt2"
    if time_stage run_z3_smt "$OUT_DIR" "$smt" "$LOG_DIR"; then
        RESULT_CATEGORY="bmc_pass"
        exit 0
    else
        RESULT_CATEGORY="bmc_fail"
    fi

    if time_stage run_z3_induct "$OUT_DIR" "$smt" "$LOG_DIR"; then
        RESULT_CATEGORY="induct_pass"
        exit 0
    else
        RESULT_CATEGORY="induct_fail"
    fi
fi

# ───── result handling ────────────────────────────────────────
case "$miter_ret:$verilator_ret" in
    "1:1") RESULT_CATEGORY="miter_fail_verilator_fail"       ;;
    "1:0") RESULT_CATEGORY="miter_fail_verilator_pass"       ;   capture_failed_seed "miter failed, but Verilator passed" "epic"; exit 0 ;;
    "1:2") RESULT_CATEGORY="miter_fail_verilator_error"      ;   capture_failed_seed "miter failed, Verilator error"      "rare"; exit 1 ;;
    "3:1") RESULT_CATEGORY="miter_timeout_verilator_fail"    ;;
    "3:2") RESULT_CATEGORY="miter_timeout_verilator_error"   ;   capture_failed_seed "miter timeout, Verilator error"     "rare"; exit 1 ;;
    "3:0") RESULT_CATEGORY="miter_timeout_verilator_pass"    ;   exit 0 ;;
    *)     RESULT_CATEGORY="miter_unknown_verilator_unknown" ;   capture_failed_seed "miter unknown, Verilator unknown"   "rare"; exit 1 ;;
esac

# ───── Reduction of failed seeds ─────────────────────────────────

reduction_out_dir="$OUT_DIR/reduction"
reduction_log_dir="$reduction_out_dir/logs"
reduction_success=0
reset=0
iteration=0

mkdir -p "$reduction_out_dir"
mkdir -p "$reduction_log_dir"

reduction_src_json="$OUT_DIR/$FUZZED_TOP.json"

while true; do

    info "running reduction iteration $iteration"

    reduction_ret=0
    time_stage run_reduction                     \
                    "$reduction_out_dir"         \
                    "$reduction_src_json"        \
                    "$LOG_DIR/verilator_run.log" \
                    "$reduction_success"         \
                    "$reset"                     \
                    "$FUZZED_TOP"                \
                    "$reduction_log_dir" || reduction_ret=$?
    reset=0
                
    reduction_src_json="$reduction_out_dir/$FUZZED_TOP.json"

    wns=$(scripts/get_wns_before_marker.py "$LOG_DIR/vivado.log")

    case $reduction_ret in
        0) ;;
        1) RESULT_CATEGORY="reduction_fail"      ; capture_failed_seed "reduction failed" "rare"          ; exit 1 ;;
        2) RESULT_CATEGORY="reduction_minimized" ; capture_failed_seed "reduction now new bug" "legendary"; exit 0 ;;
        3)  if [[ -n $wns ]]; then
                clk_period=$(( clk_period - 0.25 - wns ))
                info "Changing clk period to $clk_period"
                reset=1
            else
                RESULT_CATEGORY="reduction_new_bug"
                capture_failed_seed "reduction found new bug" "unique"
                exit 0
            fi
            ;;
    esac

    # ───── Rerun Vivado on reduced netlist ─────────────────────────────
    vivado_ret=0
    time_stage run_impl "$reduction_out_dir" "$SYNTH_TOP" "$IMPL_TOP" "$clk_period" "$FUZZED_TOP" "$reduction_log_dir" || vivado_ret=$?


    # ───── check if reduction was successful ───────────────────────────
    reduction_success=1
    if (( vivado_ret == 0 )); then
        miter_ret=0
        time_stage run_miter "$reduction_out_dir" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR" || miter_ret=$?

        if (( miter_ret != 1 )); then
            verilator_ret=0
            time_stage run_verilator "$reduction_out_dir" "$SYNTH_TOP" "$IMPL_TOP" "$LOG_DIR" || verilator_ret=$?

            if (( verilator_ret != 1 )); then
                reduction_success=0
            fi

        fi
    elif (( vivado_ret == 1 )); then
        RESULT_CATEGORY="vivado_fail_reduced"
        capture_failed_seed "Vivado failed on reduced netlist" "rare"
        exit 1
    elif (( vivado_ret == 3 )); then
        RESULT_CATEGORY="vivado_timeout_reduced"
        capture_failed_seed "Vivado timed out on reduced netlist" "rare"
        exit 1
    fi

    iteration=$(( iteration + 1 ))

    if (( iteration >= MAX_REDUCTION_ITER )); then
        RESULT_CATEGORY="reduction_max_iter"
        capture_failed_seed "reduction reached max iterations" "rare"
        exit 1
    fi

done

exit 1