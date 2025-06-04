#!/usr/bin/env python3

import os
import re
import argparse
from collections import OrderedDict

parser = argparse.ArgumentParser(
    description="Parse Vivado synthesis and implementation log files to extract optimization tables."
)
parser.add_argument(
    "log_file",
    type=str,
    help="Path to the Vivado log file to parse."
)
parser.add_argument("--header-only", action="store_true", help="Only output the header row in the CSV file.")
parser.add_argument("--header", action="store_true", help="Include header in the output CSV file.")
args = parser.parse_args()


phase_fields = [
    {
        "fields": [
            "Retarget",
            "Constant_propagation",
            "Sweep",
            "BUFG_optimization",
            "Shift_Register_Optimization",
            "Post_Processing_Netlist"
        ],
        "headers": ["created", "removed"],
        "csv_prefix": "opt_design"
    },
    {
        "fields": [
            "Retarget",
            "BUFG_optimization",
            "Remap",
            "Post_Processing_Netlist"
        ],
        "headers": ["created", "removed"],
        "csv_prefix": "power_opt_design"
    },
    {
        "fields": [
            "LUT_Combining",
            "Retime",
            "Very_High_Fanout",
            "DSP_Register",
            "Shift_Register_to_Pipeline",
            "Shift_Register",
            "BRAM_Register",
            "URAM_Register",
            "Dynamic/Static_Region_Interface_Net_Replication",
            "Total"
        ],
        "headers": ["created", "removed", "optimized"],
        "csv_prefix": "place_design"
    },
    {
        "fields": [
            "DSP_Register",
            "Critical_Path",
            "Total"
        ],
        "headers": ["wns_gain", "tns_gain", "created", "removed", "optimized"],
        "csv_prefix": "phys_opt_design_post_place"
    },
    {
        "fields": [
            "Critical_Path"
        ],
        "headers": ["wns_gain", "tns_gain", "created", "removed", "optimized"],
        "csv_prefix": "phys_opt_design_post_route"
    }
]

summary = OrderedDict()

for spec in phase_fields:
    for field in spec["fields"]:
        for header in spec["headers"]:
            key = f"{spec['csv_prefix']}.{field}.{header}"
            summary[key] = "NA"
            
            
if args.header_only or args.header:
    print(",".join(summary.keys()))

if args.header_only:
    exit(0)
    
if not os.path.exists(args.log_file):
    print(",".join(summary.values()))
    exit(0)

with open(args.log_file, "r") as f:
    log = f.read()

log = re.sub(r'[ \t]+', ' ', log)

pattern = re.compile(r"(-{40,}(\n.*){1,14}\n-{40,})")

matches = list(pattern.finditer(log))

for k, match in enumerate(matches):
    content = match.group(0)
    lines = [l.strip() for l in content.splitlines() if l.strip().startswith("|")]
    if not lines: continue

    csv_name = phase_fields[k]["csv_prefix"]
    rows = lines[1:]

    for i, row in enumerate(rows):
        fields = [x.strip() for x in row.strip("|").split("|")]
        headers = [header.strip() for header in lines[0].strip("|").split("|")]
        
        csv_row_name = f"{csv_name}.{phase_fields[k]['fields'][i]}"

        for j, header in enumerate(phase_fields[k]["headers"]):
            summary[f"{csv_row_name}.{header}"] = fields[j + 1]
        

print(",".join(summary.values()))