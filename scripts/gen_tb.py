#!/usr/bin/env python3
import json, os, sys, pathlib

out_dir = os.getenv('OUTDIR')
seed = os.getenv('SEED')
cycles = os.getenv('CYCLES')

json_file = os.path.join(out_dir, "vivado.json")
tb_file = os.path.join(out_dir, "eq_top_tb.cpp")

with open(json_file, 'r') as f:
    mod = json.load(f)["modules"]["eq_top"]["ports"]
    
inputs = []
triger = None
clk = None
    
for name, port in mod.items():
    if port["direction"] == "input":
        if 'clk' in name:
            clk = name
        else:
            converted_name = name.replace("__", "___05F")
            inputs.append(converted_name)

    elif port["direction"] == "output":
        if triger is None:
            triger = name
        else:
            SyntaxError(f"Multiple trigger ports found: {triger} and {name}")
    else:
        raise ValueError(f"Unknown port direction: {port['direction']}")
    
if triger is None:
    raise ValueError("No trigger port found")

if len(inputs) == 0:
    raise ValueError("No input ports found")

if clk is None:
    raise ValueError("No clock port found")

with open(tb_file,"w") as tb:
    tb.write( "#include <verilated.h>\n")
    tb.write( "#include <Veq_top.h>\n")
    tb.write( "#include <random>\n")
    tb.write( "#include <iostream>\n\n\n")
    
    tb.write( "int main(int argc, char **argv) {\n")
    tb.write( "    Verilated::commandArgs(argc, argv);\n\n")
    tb.write(f"    uint32_t seed = {seed};\n")
    tb.write(f"    uint32_t cycles = {cycles};\n\n")
    tb.write( "    std::mt19937 rng(seed);\n\n")
    tb.write( "    Veq_top *top = new Veq_top;\n\n")
    tb.write( "    auto rnd1 = [&]() {return rng() & 1;};\n\n")
    tb.write( "    std::cout << \"[TB] seed=\" << seed << \" cycles=\" << cycles << std::endl;\n\n")
    tb.write( "    for (int i = 0; i < cycles; i++) {\n\n")
    tb.write(f"        top->{clk} = 0;\n")
    tb.write( "        top->eval();\n\n")
    
    for name in inputs:
        tb.write(f"        top->{name} = rnd1();\n")
    
    tb.write( "\n")    
    tb.write(f"        top->{clk} = 1;\n")
    tb.write( "        top->eval();\n\n")
    
    tb.write(f"        if (top->{triger}) {{\n")
    tb.write( "            std::cerr << \"[TB] Triggered at cycle \" << i << std::endl;\n")
    tb.write( "            return 1;\n")
    tb.write( "        }\n\n")
    tb.write( "    }\n\n")
    tb.write( "    std::cerr << \"[TB] PASS (\" << cycles << \" cycles)\" << std::endl;\n\n")
    tb.write( "    delete top;\n")
    tb.write( "    return 0;\n\n")
    tb.write( "}\n") 
    