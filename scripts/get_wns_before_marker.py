#!/usr/bin/env python3
"""
Extract the WNS value that appears immediately BEFORE either of these messages:
  1) "Design worst setup slack (WNS) is greater than or equal to 0.000 ns..."
  2) "Physical Optimization has determined that the magnitude of the negative slack is too large..."

Usage:
  python get_wns_before_marker.py path/to/vivado.log
  # or:
  cat path/to/vivado.log | python get_wns_before_marker.py
"""

import sys
import re
import argparse

# Minimal distinctive substrings to robustly match the target messages.
MARKERS = [
    "Design worst setup slack (WNS) is greater than or equal to 0.000 ns",
    "magnitude of the negative slack is too large",
]

WNS_RE = re.compile(r"WNS\s*=\s*([+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?)")

def get_wns_before_marker(lines):
    last_wns = None
    for line in lines:

        m = WNS_RE.search(line)
        if m:
            last_wns = m.group(1)

        if any(marker in line for marker in MARKERS):
            return last_wns

    return None

def main():
    parser = argparse.ArgumentParser(description="Get WNS immediately before key phys_opt messages.")
    parser.add_argument("logfile", nargs="?", help="Vivado log file (defaults to stdin)")
    args = parser.parse_args()

    if args.logfile:
        try:
            with open(args.logfile, "r", encoding="utf-8", errors="ignore") as f:
                wns = get_wns_before_marker(f)
        except FileNotFoundError:
            sys.exit(1)
    else:
        wns = get_wns_before_marker(sys.stdin)

    if wns is None:
        sys.exit(0)

    print(wns)

if __name__ == "__main__":
    main()
