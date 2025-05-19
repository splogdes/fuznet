#!/usr/bin/env bash
# -------------------------------------------------------------------
# ARGS (override from command-line or env)
NETLIST_RTL=${NETLIST_RTL:-output/post_synth.v} 
NETLIST_PNR=${NETLIST_PNR:-output/post_impl.v} 
TOP=${TOP:-top}
LOG_DIR=${LOG_DIR:-output/logs}                                 
OUTDIR=${OUTDIR:-output}                          
PRIMS=${PRIMS:-+/xilinx/cells_map.v +/xilinx/cells_sim.v}
LIBRARY=${LIBRARY:-hardware/cells/xilinx.yaml}
CONFIG=${CONFIG:-config/settings.toml}
XILINX_TCL=${XILINX_TCL:-flows/vivado/impl.tcl}

FUZZED_NETLIST=${OUTDIR}/fuzzed_netlist

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m'


pass_msg() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

error_msg() {
    echo -e "${RED}[ERROR]${NC} $1"
}

warn_msg() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

info_msg() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

SEED=$RANDOM

mkdir -p "$OUTDIR"
mkdir -p "$LOG_DIR"

DATE=$(date +%Y-%m-%d_%H-%M-%S)


info_msg "Running equivalence check for $TOP"
info_msg "RTL netlist: $NETLIST_RTL"
info_msg "PNR netlist: $NETLIST_PNR"
info_msg "Primitives: $PRIMS"
info_msg "Output directory: $OUTDIR/"
info_msg "Log directory: $LOG_DIR/"
info_msg "Seed: $SEED"

info_msg "Building fuznet"

./scripts/build.sh > /dev/null 2>&1

if [ $? -ne 0 ]; then
    error_msg "Build failed"
    exit 1
fi

info_msg "Build completed"

info_msg "Running fuznet"

./build/fuznet -l $LIBRARY -c $CONFIG -s $SEED -o $FUZZED_NETLIST

info_msg "Running PnR with Vivado"
vivado -mode batch -log $LOG_DIR/${DATE}_vivado.log -journal $LOG_DIR/${DATE}_vivado.jou -source $XILINX_TCL \
    -tclargs $NETLIST_RTL $NETLIST_PNR $TOP $FUZZED_NETLIST.v > /dev/null 2>&1

if [ $? -ne 0 ]; then
    error_msg "PnR failed, check $LOG_DIR/"$DATE"_vivado.log"
    exit 1
fi

info_msg "Running structural equivalence check"

yosys -q -p "
    read_verilog  $NETLIST_RTL
    rename $TOP gold

    read_verilog  $NETLIST_PNR
    rename $TOP gate

    read_verilog -lib $PRIMS

    equiv_make gold gate equiv_top

    opt; opt_clean

    show -format dot -prefix $OUTDIR/01_struct
    write_verilog -noattr   $OUTDIR/equiv_top.v

    equiv_struct             equiv_top
    equiv_status   -assert   equiv_top
" > /dev/null 2>&1

if [ $? -eq 0 ]; then
    pass_msg "Structural equivalence check passed"
    pass_msg "No need for miter generation"
    exit 0
fi

warn_msg "Structural equivalence check failed"
info_msg "Running miter equivalence check"

yosys -q -l $LOG_DIR/$DATE"_miter.log" -p "
          read_verilog $NETLIST_RTL
          rename $TOP netlist

          read_verilog $NETLIST_PNR
          rename $TOP impl
         
          read_verilog $PRIMS

          proc; memory; opt
          
          miter -equiv -make_assert netlist impl eq_top
          show      -format dot -prefix $OUTDIR/01_miter_raw

          hierarchy -top eq_top; proc; flatten;
          opt_clean; proc; memory; opt

          show      -format dot -prefix $OUTDIR/02_miter_flat
          
          write_verilog -noattr $OUTDIR/vivado.v
          sat -dump_vcd $OUTDIR/counter_example.vcd -verify-no-timeout -timeout 1 -prove-asserts -tempinduct eq_top
          dffunmap
          write_smt2 $OUTDIR/vivado.smt2" > /dev/null 2>&1


status=$? 
log="$LOG_DIR/${DATE}_miter.log"

if   [[ $status -eq 0 && $(grep -c 'SUCCESS!' "$log") -eq 1 ]]; then
    pass_msg  "Miter check passed"
    pass_msg  "Equivalence check passed"
    exit 0

elif [[ $status -eq 0 && $(grep -c 'TIMEOUT!' "$log") -eq 1 ]]; then
    warn_msg  "Miter check timed out"

elif [[ $status -eq 1 && $(grep -c 'FAIL!' "$log") -eq 1 ]]; then
    error_msg "Miter check failed, log file: $log"
    error_msg "NOT EQUIVALENT"
    exit 1

elif [[ $status -eq 1 ]]; then
    warn_msg  "No equivalence check result, log file: $log"

else
    error_msg "INTERNAL ERROR: Unknown error, log file: $log"

fi

info_msg "Running BMC and Induction check"

yosys-smtbmc -s z3 -t 1000 --dump-vcd $OUTDIR/vivado_bmc.vcd $OUTDIR/vivado.smt2 > $LOG_DIR/$DATE"_bmc.log"

if [ $? -ne 0 ]; then
    error_msg "BMC check failed"
    error_msg "NOT EQUIVALENT"
    exit 1
fi

info_msg "BMC check passed"
info_msg "Running Induction check"

yosys-smtbmc -s z3 -t 128 -i --timeout 60 --dump-vcd $OUTDIR/vivado_tmp.vcd $OUTDIR/vivado.smt2 > $LOG_DIR/$DATE"_induction.log"

if [ $? -eq 0 ]; then
    pass_msg "BMC and Induction check passed"
    pass_msg "Equivalence check passed"
    exit 0
fi

warn_msg "BMC passed but Induction check failed, log file: $LOG_DIR/${DATE}_induction.log"
warn_msg "No equivalence check result"
exit 2