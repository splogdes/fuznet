#!/usr/bin/env bash
# Arguments: 1: synth_top, 2: impl_top, 3: out, 4: log_dir (optional)
# Returns: 0 on pass, 1 on failure, 2 on error

run_verilator() {
    local out=$1
    local synth_top=$2
    local impl_top=$3
    local log_dir=${4:-"$out/logs"}
    
    info "Running Verilator Simulator for equivalence checking"
    local cpp_tb="eq_top_tb.cpp"

    if ! ./scripts/gen_miter.py          \
            --outdir "$out"              \
            --seed "$SEED"               \
            --gold-top "$synth_top"      \
            --gate-top "$impl_top"       \
            --tb "$cpp_tb"               \
            --cycles 1000;
    then
        fail "Failed to generate miter testbench"
        return 2
    fi

    if ! verilator -cc --exe --build     \
            -DGLBL -Wno-fatal -I"$out"   \
            --trace                      \
            --trace-underscore           \
            -Mdir "$out/build"           \
            "$out/eq_top.v"              \
            "$PRIMS_V"                   \
            "$out/$cpp_tb"               \
            > "$log_dir/verilator.log" 2>&1
    then
        fail "Verilator build failed"
        return 2
    fi

    if "$out/build/Veq_top" >> "$log_dir/verilator_run.log" 2>&1; then
        pass "Verilator equivalence check passed"
        return 0
    else
        fail "Verilator equivalence check failed"
        return 1
    fi

}