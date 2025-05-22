#!/usr/bin/env bash
# Endless driver: keeps launching one fuzz-PnR-equiv cycle after another.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$PROJECT_ROOT"

if [[ -n "${YOSYS_ROOT:-}" ]]; then
    echo "[INFO] YOSYS_ROOT is set to $YOSYS_ROOT"
    export PATH="$YOSYS_ROOT:$PATH"
fi

workers=${FUZNET_WORKERS:-1}
echo "[POOL] launching $workers workers…"

for i in $(seq $workers); do
    echo "[POOL] launching worker $i"
    (
        current_sleep=1
        export WORKER_ID=$i
        
        sleep $((RANDOM % 10))
        
        while true; do
        
            if ! ./scripts/fuzzer.sh 2>&1 | sed -u "s/^/[w$i] /"; then
                echo "[w$i] fuzzer.sh failed, retrying in $current_sleep seconds…"
                sleep $current_sleep
                current_sleep=$((current_sleep * 2))
            fi

        done
    ) &
done

wait