#!/usr/bin/env bash
# Arguments: 1: synth_top, 2: impl_top, 3: out, 4: log_dir (optional)
# Returns: 0 on pass, 1 on failure, 2 on unknown state, 3 on timeout

run_miter() {
    local out=$1
    local synth_top=$2
    local impl_top=$3
    local log_dir=${4:-"$out/logs"}

    info "Running miter for equivalence check"

    local yosys_script="$out/miter.ys"
    local template="flows/yosys/miter.ys.in"

    sed -e "s|__SYNTH__|$synth_top|g"   \
        -e "s|__IMPL__|$impl_top|g"     \
        -e "s|__TOP__|$TOP|g"           \
        -e "s|__PRIMS__|$PRIMS_V|g"     \
        -e "s|__OUT__|$out|g"           \
        "$template" > "$yosys_script"

    local miter_log="$log_dir/miter.log"
    local miter_ret=0
    yosys -q -l "$miter_log" -s "$yosys_script" \
          > /dev/null 2>&1 || miter_ret=$?

    local miter_token=$(grep -oE 'SUCCESS!|FAIL!|TIMEOUT!' "$miter_log" || echo "UNKNOWN")

    case "$miter_token:$miter_ret" in
        "SUCCESS!:0") pass  "Miter check passed"        ; return 0 ;;
        "FAIL!:1")    fail  "Miter check failed"        ; return 1 ;;
        "TIMEOUT!:0") warn  "Miter check timed out"     ; return 3 ;;
        *)            fail  "Miter check unknown state" ; return 2 ;;
    esac
}