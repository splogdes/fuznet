#!/usr/bin/env python3
import argparse
import json
import os
import sys

def parse_args():
    p = argparse.ArgumentParser(
        description="Generate Verilator testbench and top wrapper for eq_top module."
    )
    p.add_argument("--outdir",  required=True, help="Output directory")
    p.add_argument("--seed",    required=True, help="Random seed")
    p.add_argument("--cycles",  required=True, type=int, help="Number of cycles to simulate")
    p.add_argument("--json",    required=True, help="Path to JSON file with module ports")
    p.add_argument("--gold-top", required=True, help="Name of the golden (RTL) top module")
    p.add_argument("--gate-top", required=True, help="Name of the gate-level top module")
    p.add_argument("--tb",      required=True, help="Filename for the generated testbench")
    p.add_argument(
        "--no-vcd",
        action="store_true",
        help="If set, the generated testbench will not include VCD tracing"
    )
    return p.parse_args()

def load_ports(json_path):
    data = json.load(open(json_path))
    module = list(data["modules"].keys())[0]
    ports = data["modules"][module]["ports"]
    clk = None
    inputs, outputs = [], []
    for name, info in ports.items():
        clean = name.replace("__", "___05F")
        if info["direction"] == "input":
            if "clk" in name:
                clk = name
            else:
                inputs.append(clean)
        elif info["direction"] == "output":
            outputs.append(clean)
        else:
            raise ValueError(f"Unknown port direction: {info['direction']}")
    if clk is None:
        raise ValueError("No clock port found")
    if not inputs:
        raise ValueError("No input ports found")
    if not outputs:
        raise ValueError("No output ports found")
    return clk, inputs, outputs

def write_eq_top(outdir, clk, inputs, outputs, gate_top, gold_top):
    path = os.path.join(outdir, "eq_top.v")
    with open(path, "w") as f:
        f.write("module eq_top(\n")
        # Port list
        all_ports = [clk] + inputs + ["trigger"]
        for p in all_ports[:-1]:
            f.write(f"    input wire {p},\n")
        f.write("    output wire trigger\n);\n\n")

        # Internal signals
        for o in outputs:
            f.write(f"    wire {o}_{gate_top};\n")
            f.write(f"    wire {o}_{gold_top};\n")
            f.write(f"    wire {o};\n")
        f.write("    wire equivalent;\n\n")

        # Instantiate gate-level
        f.write(f"    {gate_top} inst_{gate_top} (\n")
        f.write(f"        .clk({clk}),\n")
        for inp in inputs:
            f.write(f"        .{inp}({inp}),\n")
        for i, out in enumerate(outputs):
            comma = "," if i < len(outputs)-1 else ""
            f.write(f"        .{out}({out}_{gate_top}){comma}\n")
        f.write("    );\n\n")

        # Instantiate golden (RTL)
        f.write(f"    {gold_top} inst_{gold_top} (\n")
        f.write(f"        .clk({clk}),\n")
        for inp in inputs:
            f.write(f"        .{inp}({inp}),\n")
        for i, out in enumerate(outputs):
            comma = "," if i < len(outputs)-1 else ""
            f.write(f"        .{out}({out}_{gold_top}){comma}\n")
        f.write("    );\n\n")

        # Compare outputs
        for out in outputs:
            f.write(f"    assign {out} = ({out}_{gate_top} === {out}_{gold_top});\n")
        f.write("\n")
        f.write("    assign equivalent = &{ " + ", ".join(outputs) + " };\n")
        f.write("    assign trigger = ~equivalent;\n\n")
        f.write("endmodule\n")

def write_testbench(outdir, tb_name, clk, inputs, seed, cycles, no_vcd=False):
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
        if not no_vcd:
            tb.write("            tfp->close();\n")
        tb.write("            return 1;\n        }\n    }\n\n")
        tb.write("    std::cerr << \"[TB] PASS (\" << cycles << \" cycles)\" << std::endl;\n")
        if not no_vcd:
            tb.write("    tfp->close();\n")
        tb.write("    return 0;\n}\n")

def main():
    args = parse_args()
    os.makedirs(args.outdir, exist_ok=True)

    clk, inputs, outputs = load_ports(os.path.join(args.outdir, args.json))
    write_eq_top(args.outdir, clk, inputs, outputs, args.gate_top, args.gold_top)
    write_testbench(args.outdir, args.tb, clk, inputs, args.seed, args.cycles, args.no_vcd)

if __name__ == "__main__":
    main()
