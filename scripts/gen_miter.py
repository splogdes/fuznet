#!/usr/bin/env python3
import argparse
import os
import sys
import re

PORT_RE  = re.compile(r'^\s*(input|output)\s+(?:wire\s+)?([A-Za-z_][A-Za-z_0-9]*)')

def parse_args():
    p = argparse.ArgumentParser(
        description="Generate Verilator testbench and top wrapper for eq_top module."
    )
    p.add_argument("--outdir",   required=True, help="Output directory")
    p.add_argument("--seed",     required=True, help="Random seed")
    p.add_argument("--cycles",   required=True, type=int, help="Number of cycles to simulate")
    p.add_argument("--gold-top", required=True, help="Name of the golden (RTL) top module")
    p.add_argument("--gate-top", required=True, help="Name of the gate-level top module")
    p.add_argument("--tb",       required=True, help="Filename for the generated testbench")
    p.add_argument(
        "--no-vcd",
        action="store_true",
        help="If set, the generated testbench will not include VCD tracing"
    )
    return p.parse_args()

def load_ports(path):
    """
    Parse a single-module netlist of the form Vivado writes:
        module synth (port0, port1, ...);
        input  port0;
        output port1;
    Returns (clk, inputs, outputs)
    """
    clk      = None
    inputs   = []
    outputs  = []

    with open(path) as f:
        in_header = False
        for line in f:
            if line.lstrip().startswith("module"):
                in_header = True
                continue
            if in_header and ");" in line:   # end of port list
                in_header = False
                continue
            m = PORT_RE.match(line)
            if not m:
                continue
            direction, name = m.groups()
            clean = name.replace("__", "___05F")
            if direction == "input":
                if name == "clk" or "clk" in name:
                    clk = clean
                else:
                    inputs.append(clean)
            elif direction == "output":
                outputs.append(clean)

    if clk is None:
        raise ValueError("Clock not found in {}".format(path))
    if not inputs:
        raise ValueError("No inputs found in {}".format(path))
    if not outputs:
        raise ValueError("No outputs found in {}".format(path))
    return clk, inputs, outputs

def write_eq_top(outdir, clk, inputs, outputs, gate_top, gold_top):
    path = os.path.join(outdir, "eq_top.v")
    with open(path, "w") as f:
        f.write("module eq_top(\n")

        input_ports = [clk] + inputs
        for p in input_ports:
            f.write(f"    input wire {p},\n")
        for o in outputs:
            f.write(f"    output wire {o},\n")
        f.write("    output wire trigger\n);\n\n")

        for o in outputs:
            f.write(f"    wire {o}_{gate_top};\n")
            f.write(f"    wire {o}_{gold_top};\n")
        f.write("    wire equivalent;\n\n")

        f.write(f"    {gate_top} inst_{gate_top} (\n")
        f.write(f"        .clk({clk}),\n")
        for inp in inputs:
            f.write(f"        .{inp}({inp}),\n")
        for i, out in enumerate(outputs):
            comma = "," if i < len(outputs)-1 else ""
            f.write(f"        .{out}({out}_{gate_top}){comma}\n")
        f.write("    );\n\n")

        f.write(f"    {gold_top} inst_{gold_top} (\n")
        f.write(f"        .clk({clk}),\n")
        for inp in inputs:
            f.write(f"        .{inp}({inp}),\n")
        for i, out in enumerate(outputs):
            comma = "," if i < len(outputs)-1 else ""
            f.write(f"        .{out}({out}_{gold_top}){comma}\n")
        f.write("    );\n\n")

        for out in outputs:
            f.write(f"    assign {out} = ({out}_{gate_top} === {out}_{gold_top});\n")
        f.write("\n")
        f.write("    assign equivalent = &{ " + ", ".join(outputs) + " };\n")
        f.write("    assign trigger = ~equivalent;\n\n")
        f.write("endmodule\n")

def write_testbench(outdir, tb_name, clk, inputs, outputs, seed, cycles, no_vcd=False):
    path = os.path.join(outdir, tb_name)
    with open(path, "w") as tb:
        tb.write("#include <verilated.h>\n")
        if not no_vcd:
            tb.write("#include <verilated_vcd_c.h>\n")
        tb.write("#include <Veq_top.h>\n")
        tb.write("#include <random>\n#include <iostream>\n\n")

        tb.write("int main(int argc, char **argv) {\n")
        tb.write("    Verilated::commandArgs(argc, argv);\n")
        if not no_vcd:
            tb.write("    VerilatedVcdC* tfp = new VerilatedVcdC;\n")
        tb.write("    Veq_top* top = new Veq_top;\n\n")

        if not no_vcd:
            tb.write("    Verilated::traceEverOn(true);\n")
            tb.write("    top->trace(tfp, 99);\n")
            tb.write(f"    tfp->open(\"{outdir}/eq_top.vcd\");\n\n")

        tb.write(f"    uint32_t seed = {seed};\n")
        tb.write(f"    uint32_t cycles = {cycles};\n")
        tb.write("    std::mt19937 rng(seed);\n")
        tb.write("    auto rnd_bit = [&]() { return rng() & 1; };\n\n")
        tb.write("    std::cerr << \"[TB] seed=\" << seed << \" cycles=\" << cycles << std::endl;\n\n")
        tb.write("    bool trigger = false;\n\n")
        tb.write("    for (uint32_t i = 0; i < cycles; ++i) {\n")
        tb.write(f"        top->{clk} = 0;\n        top->eval();\n")
        if not no_vcd:
            tb.write("        tfp->dump(i * 10);\n")
        for inp in inputs:
            tb.write(f"        top->{inp} = rnd_bit();\n")
        tb.write(f"\n        top->{clk} = 1;\n        top->eval();\n")
        if not no_vcd:
            tb.write("        tfp->dump(i * 10 + 5);\n")
        tb.write("        if (top->trigger) {\n")
        tb.write("            std::cerr << \"[TB] Triggered at cycle \" << i << std::endl;\n")
        for out in outputs:
            tb.write(f"            if (!top->{out}) std::cout << \"[TB] Triggered by wire {out} \" << std::endl;\n")
        tb.write("            trigger=true;\n        }\n    }\n\n")
        tb.write("    std::cerr << \"[TB] PASS (\" << cycles << \" cycles)\" << std::endl;\n")
        if not no_vcd:
            tb.write("    tfp->close();\n")
        tb.write("    delete top;\n")
        if not no_vcd:
            tb.write("    delete tfp;\n")
        tb.write("    if (trigger)\n")
        tb.write("        return 1;\n")
        tb.write("    return 0;\n")
        tb.write("}\n")

def main():
    args = parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    clk, inputs, outputs = load_ports(os.path.join(args.outdir, args.gold_top + ".v"))
    write_eq_top(args.outdir, clk, inputs, outputs, args.gate_top, args.gold_top)
    write_testbench(args.outdir, args.tb, clk, inputs, outputs, args.seed, args.cycles, args.no_vcd)

if __name__ == "__main__":
    main()
