#!/usr/bin/env bash
# Arguments:  synth_top impl_top out_dir [log_dir]
# Returns:    0 on success, 1 on failure

run_struct() {
    local out=$1
    local synth_top=$2
    local impl_top=$3
    local log_dir=${4:-"$out/logs"}

    info "Running Structural equivalence check"
    local yosys_script="$out/struct_check.ys"
    local template="flows/yosys/struct.ys.in"

    sed -e "s|__SYNTH__|$synth_top|g"     \
        -e "s|__IMPL__|$impl_top|g"       \
        -e "s|__TOP__|$TOP|g"             \
        -e "s|__PRIMS__|$PRIMS_V|g"       \
        -e "s|__OUT__|$out|g"             \
        "$template" > "$yosys_script"

    if yosys -q -l "$log_dir/struct.log" -s "$yosys_script" \
          > /dev/null 2>&1; then
        pass "Structural equivalence check passed"
        return 0
    else
        warn "Structural equivalence check failed"
        return 1
    fi
}