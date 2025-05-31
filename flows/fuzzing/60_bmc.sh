#!/usr/bin/env bash
# arguments:  out_dir smt_file [log_dir]
# returns:    0 on success, 1 on failure, 2 on error, 3 on timeout

run_z3_smt() {
    local out=$1
    local smt_file=$2
    local log_dir=${2:-"$out/logs"}

    info "BMC (Z3, 1000 steps, timeout 60s)"
    local bmc_log="$log_dir/z3_bmc.log"
    if ! yosys-smtbmc -s z3 -t 1000 --timeout 60 --dump-vcd "$out/bmc.vcd" "$out/eq_top.smt2" \
               > "$bmc_log" 2>&1; then
        case $(grep -oE 'timeout|FAILED' "$bmc_log" || echo "UNKNOWN") in
            "timeout") warn  "BMC timed out"        ; return 3 ;;
            "FAILED")  fail  "BMC failed"           ; return 1 ;;
            *)         fail  "BMC unknown state"    ; return 2 ;;
        esac
    fi
    pass "BMC completed successfully"
    return 0
}