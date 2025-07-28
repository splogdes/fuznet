#!/usr/bin/env bash
# Arguments:  out_dir  fuzzed_netlist_json  verilator_run_log  last_reduction_successful  [fuzzed_top]  [log_dir]
# Returns:  0 on success, 1 on failure, 2 if no new bugs found, 3 if new bug found

run_reduction() {
    local out=$1
    local fuzzed_netlist_json=$2
    local verilator_run_log=$3
    local last_reduction_successful=$4
    local fuzzed_top=${5:-"fuznet_netlist"}
    local log_dir=${6:-"$out/logs"}

    mkdir -p "$log_dir"
    info "Running netlist reduction on $fuzzed_netlist_json"

    wire_to_keep=$(
        grep "\[TB\] Triggered by wire" "$verilator_run_log" \
            | awk 'NR==1 {gsub(/[^0-9]/, "", $NF); print $NF + 0}'
    )

    args=(
        reduce
        -l "$CELL_LIB"
        -s "$SEED"
        -i "$fuzzed_netlist_json"
        -r "$wire_to_keep"
        -o "$out/$fuzzed_top"
        -j
        -v
        --hash-file "$HASH_FILE"
    )

    flags=""
    if (( last_reduction_successful )); then
        args+=("--last-success")
    fi

    fuznet_ret=0
    "$FUZNET_BIN"  "${args[@]}" \
                    >> "$log_dir/fuznet_reduction.log" 2>&1 \
                    || fuznet_ret=$?
    
    if (( fuznet_ret == 0 )); then
        info "fuznet reduction completed successfully"
        return 0
    elif (( fuznet_ret == 1 )); then
        info "fuznet reduction failed"
        return 1
    elif (( fuznet_ret == 2 )); then
        info "fuznet reduction did not find any new bugs"
        return 2
    elif (( fuznet_ret == 3 )); then
        info "fuznet reduction found a new bug"
        return 3
    fi
}
                      