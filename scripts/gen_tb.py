#!/usr/bin/env python3
import json, os,argparse

parser = argparse.ArgumentParser(description="Generate testbench for eq_top module.")
parser.add_argument("--outdir", required=True, help="Output directory")
parser.add_argument("--seed", required=True, type=int, help="Random seed")
parser.add_argument("--cycles", required=True, type=int, help="Number of cycles")
parser.add_argument("--json", required=True, help="Path to the JSON file containing module information")
parser.add_argument("--gold-top", required=True, help="Name of the top module for golden output")
parser.add_argument("--gate-top", required=True, help="Name of the top module for gate-level simulation")
parser.add_argument("--tb", required=True, help="Name of the testbench file")


args = parser.parse_args()

out_dir = args.outdir
seed = args.seed
cycles = args.cycles

json_file = args.json
tb_file = os.path.join(out_dir, args.tb)

with open(json_file, 'r') as f:
    mod = json.load(f)["modules"]["post_synth"]["ports"]
    
inputs = []
outputs = []
clk = None
    
for name, port in mod.items():
    if port["direction"] == "input":
        if 'clk' in name:
            clk = name
        else:
            converted_name = name.replace("__", "___05F")
            inputs.append(converted_name)

    elif port["direction"] == "output":
        converted_name = name.replace("__", "___05F")
        outputs.append(converted_name)
        
    else:
        raise ValueError(f"Unknown port direction: {port['direction']}")

if len(inputs) == 0:
    raise ValueError("No input ports found")

if len(outputs) == 0:
    raise ValueError("No output ports found")

if clk is None:
    raise ValueError("No clock port found")

gate_top = args.gate_top
gold_top = args.gold_top

with open(os.path.join(out_dir, "eq_top.v"), "w") as f:
    f.write( "module eq_top(\n")
    for name in [clk] + inputs:
        f.write(f"    input wire {name},\n")
    f.write( "    output wire trigger\n")
    f.write( ");\n\n")
    f.write( "    // Internal signals\n")
    for name in outputs:
        f.write(f"    wire {name}_{gate_top};\n")
        f.write(f"    wire {name}_{gold_top};\n")
        f.write(f"    wire {name};\n")
    f.write( "    wire equivalent;\n\n")
    f.write( "    // Instantiate gate-level module\n")
    f.write(f"    {gate_top} {gate_top}_inst (\n")
    f.write(f"        .clk({clk}),\n")
    for i, name in enumerate(inputs):
        f.write(f"        .{name}({name}),\n")
    for i, name in enumerate(outputs):
        f.write(f"        .{name}({name}_{gate_top})")
        if i < len(outputs) - 1:
            f.write( ",\n")
        else:
            f.write( "\n")
    f.write( "    );\n\n")
    f.write( "    // Instantiate golden module\n")
    f.write(f"    {gold_top} {gold_top}_inst (\n")
    f.write(f"        .clk({clk}),\n")
    for i, name in enumerate(inputs):
        f.write(f"        .{name}({name}),\n")
    for i, name in enumerate(outputs):
        f.write(f"        .{name}({name}_{gold_top})")
        if i < len(outputs) - 1:
            f.write( ",\n")
        else:
            f.write( "\n")
    f.write( "    );\n\n")
    f.write( "    // Compare outputs\n")
    for name in outputs:
        f.write(f"    assign {name} = {name}_{gate_top} === {name}_{gold_top};\n")
    f.write( "\n")
    f.write("    assign equivalent = & {")
    for i, name in enumerate(outputs):
        f.write(f" {name}")
        if i < len(outputs) - 1:
            f.write(", ")
    f.write(" };\n\n")
    f.write( "    assign trigger = ~equivalent;\n\n")
    f.write( "endmodule\n")    

with open(tb_file,"w") as tb:
    tb.write( "#include <verilated.h>\n")
    tb.write( "#include <verilated_vcd_c.h>\n")
    tb.write( "#include <Veq_top.h>\n")
    tb.write( "#include <random>\n")
    tb.write( "#include <iostream>\n\n\n")
    
    tb.write( "int main(int argc, char **argv) {\n")
    tb.write( "    Verilated::commandArgs(argc, argv);\n")
    tb.write( "    VerilatedVcdC *tfp = new VerilatedVcdC;\n")
    tb.write( "    Veq_top *top = new Veq_top;\n\n")
    tb.write( "    Verilated::traceEverOn(true);\n")
    tb.write( "    top->trace(tfp, 99);\n")
    tb.write( "    tfp->open(\"eq_top.vcd\");\n\n")
    tb.write(f"    uint32_t seed = {seed};\n")
    tb.write(f"    uint32_t cycles = {cycles};\n\n")
    tb.write( "    std::mt19937 rng(seed);\n\n")
    tb.write( "    auto rnd1 = [&]() {return rng() & 1;};\n\n")
    tb.write( "    std::cout << \"[TB] seed=\" << seed << \" cycles=\" << cycles << std::endl;\n\n")
    tb.write( "    for (int i = 0; i < cycles; i++) {\n\n")
    tb.write(f"        top->{clk} = 0;\n")
    tb.write( "        top->eval();\n\n")
    tb.write( "        tfp->dump(i * 10);\n\n")
    
    for name in inputs:
        tb.write(f"        top->{name} = rnd1();\n")
    
    tb.write( "\n")    
    tb.write(f"        top->{clk} = 1;\n")
    tb.write( "        top->eval();\n\n")
    tb.write( "        tfp->dump(i * 10 + 5);\n\n")
    
    tb.write(f"        if (top->trigger) {{\n")
    tb.write( "            std::cerr << \"[TB] Triggered at cycle \" << i << std::endl;\n")
    tb.write( "            return 1;\n")
    tb.write( "        }\n\n")
    tb.write( "    }\n\n")
    tb.write( "    std::cerr << \"[TB] PASS (\" << cycles << \" cycles)\" << std::endl;\n\n")
    tb.write( "    return 0;\n\n")
    tb.write( "}\n") 
    