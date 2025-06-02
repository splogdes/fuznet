# Define inputs
set part xc7a35ticsg324-1L
set out_dir [lindex $argv 0]
set synth_top [lindex $argv 1]
set impl_top  [lindex $argv 2]
set fuzz_top [lindex $argv 3]
set netlist [lindex $argv 4]

# Load Verilog netlist
read_verilog $netlist

# Open (link) the design — must be after reading Verilog
link_design -part $part -top $fuzz_top

write_verilog -rename_top $synth_top -force -mode funcsim $out_dir/$synth_top\.v

# Now you can do P&R
opt_design
power_opt_design

place_design
phys_opt_design
power_opt_design

route_design
phys_opt_design

# Export outputs
write_verilog -rename_top $impl_top -force -mode funcsim $out_dir/$impl_top\.v
# write_sdf     -force              netlist/post_impl.sdf
# write_xdc     -no_fixed_only -exclude_timing -add_netlist_placement \
#                -force netlist/impl_placement.xdc

quit
