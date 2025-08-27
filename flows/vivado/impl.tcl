# Define inputs
set part xc7a35ticsg324-1L
set out_dir [lindex $argv 0]
set synth_top [lindex $argv 1]
set impl_top  [lindex $argv 2]
set fuzz_top [lindex $argv 3]
set netlist [lindex $argv 4]
set clk_period [lindex $argv 5]

# Load Verilog netlist
read_verilog $netlist

# Open (link) the design — must be after reading Verilog
link_design -part $part -top $fuzz_top

create_clock -name clk -period $clk_period [get_ports clk]

write_verilog -rename_top $synth_top -force -mode funcsim $out_dir/$synth_top\.v

# Now you can do P&R
opt_design
power_opt_design

place_design
phys_opt_design
power_opt_design

route_design
phys_opt_design

write_verilog -rename_top $impl_top -force -mode funcsim $out_dir/$impl_top\.v


quit
