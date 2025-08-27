#!/usr/bin/env bash
# Arguments:  out_dir synth_top impl_top [clk_period] [fuzed_top] [log_dir]
# Returns:    0 on success, 1 on failure, 2 on error, 3 on timeout

run_impl() {
    local out=$1
    local synth_top=$2
    local impl_top=$3
    local clk_period=${4:-10.000}
    local fuzed_top=${5:-"fuzzed_netlist"}
    local log_dir=${6:-"$out/logs"}

    info "Running Vivado implementation for $fuzed_top with clk period $clk_period"
    local vivado_ret=0
    timeout 600 "$VIVADO_BIN" -mode batch          \
                  -log "$log_dir/vivado.log"       \
                  -journal "$log_dir/vivado.jou"   \
                  -source "$VIVADO_TCL"            \
                  -tclargs "$out" "$synth_top" "$impl_top" "$TOP" "$out/$fuzed_top.v" "$clk_period" \
                  >/dev/null 2>&1 || vivado_ret=$?
    
    if (( vivado_ret == 124 )); then
        fail "Vivado implementation timed out"
        return 3
    elif (( vivado_ret > 128 )); then
        fail "Vivado crashed with signal $vivado_ret"
        return 2
    elif (( vivado_ret > 0 )); then
        fail "Vivado failed with exit code $vivado_ret"
        return 1
    fi
    rm -f clockInfo.txt || true
    rm -f tight_setup_hold_pins.txt || true
    info "Vivado implementation completed successfully"
    return 0
}