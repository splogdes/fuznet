#!/usr/bin/env bash
# arguments: out_dir smt_file [log_dir]
# returns: 0 on success, 1 on failure

run_z3_induct() {
    local out=$1
    local smt_file=$2
    local log_dir=${3:-"$out/logs"}

    info "Induction (Z3, 128 steps, timeout 60s)"
    
    if yosys-smtbmc -s z3 -i -t 128 --timeout 60 --dump-vcd "$out/z3_induct.vcd" "$smt_file" \
               > "$log_dir/z3_induct.log" 2>&1; then
        pass "Induction passed - equivalence proven"
        return 0
    else
        warn "Induction failed - see z3_induct.log"
        return 1
    fi
}