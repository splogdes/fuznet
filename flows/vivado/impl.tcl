# Define inputs
set part xc7a35ticsg324-1L
set synth_file [lindex $argv 0]
set impl_file  [lindex $argv 1]
set top [lindex $argv 2]
set netlist [lindex $argv 3]

# Load Verilog netlist
read_verilog $netlist

# Open (link) the design — must be after reading Verilog
link_design -part $part -top $top

write_verilog  -force -mode funcsim $synth_file

# Now you can do P&R
opt_design
place_design
route_design

# Export outputs
write_verilog -force -mode funcsim $impl_file
# write_sdf     -force              netlist/post_impl.sdf
# write_xdc     -no_fixed_only -exclude_timing -add_netlist_placement \
#                -force netlist/impl_placement.xdc

quit
