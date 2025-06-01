#!/usr/bin/env bash

run_reduction() {
    local out=$1
    local fuzzed_netlist_json=$2
    local verilator_run_log=$3
    local fuzzed_top=${4:-"fuznet_netlist"}
    local log_dir=${5:-"$out/logs"}

    mkdir -p "$log_dir"
    info "Running netlist reduction on $fuzzed_netlist_json"

    wire_to_keep=$(
        grep "\[TB\] Triggered by wire" "$verilator_run_log" \
            | awk 'NR==1 {gsub(/[^0-9]/, "", $NF); print $NF + 0}'
    )

    if "$FUZNET_BIN"  reduce                       \
                      -l "$CELL_LIB"               \
                      -s "$SEED"                   \
                      -i "$fuzzed_netlist_json"    \
                      -r "$wire_to_keep"           \
                      -o "$out/$fuzzed_top"        \
                      -v                           \
                      > "$log_dir/fuznet_reduction.log" 2>&1;
    then
        info "FuzNet reduction completed successfully"
        return 0
    else
        fail "FuzNet reduction failed"
        return 1
    fi
}
                      