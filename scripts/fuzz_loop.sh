#!/usr/bin/env bash
# Endless driver: keeps launching one fuzz-PnR-equiv cycle after another.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$PROJECT_ROOT"

if [[ -n "${YOSYS_DIR:-}" ]]; then
    echo "YOSYS_DIR is set to $YOSYS_DIR"
    export PATH="$YOSYS_DIR:$PATH"
fi

while true; do

    scripts/fuzzer.sh || true

    sleep 2

done
