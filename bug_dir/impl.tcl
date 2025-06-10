# Define inputs
set part xc7a35ticsg324-1L
set out_dir [lindex $argv 0]

read_verilog $out_dir/bug.v

link_design -part $part -top top

write_verilog -rename_top synth -force -mode funcsim $out_dir/synth.v

opt_design

write_verilog -rename_top impl -force -mode funcsim $out_dir/impl.v

quit
