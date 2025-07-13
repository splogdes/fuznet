#!/usr/bin/env bash
# Arguments:  out_dir  [seed]  [log_dir]
# Returns:    0 on success, 1 on failure 

run_gen() {
    local out=$1
    local fuzed_top=${2:-"fuznet_netlist"}
    local log_dir=${3:-"$out/logs"}

    info "Generating fuzzed netlist with fuznet"

    if "$FUZNET_BIN"  generate                     \
                      -l "$CELL_LIB"               \
                      -c "$SETTINGS_TOML"          \
                      -s "$SEED"                   \
                      -v                           \
                      -j                           \
                      -a                           \
                      -o "$out/$fuzed_top"         \
                      >"$log_dir/fuznet.log" 2>&1;
    then
        info "fuznet generation completed successfully"
        return 0
    else
        fail "fuznet generation failed"
        return 1
    fi
}