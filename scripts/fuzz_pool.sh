#!/usr/bin/env bash
# Endless driver: keeps launching one fuzz-PnR-equiv cycle after another.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$PROJECT_ROOT"

workers=${FUZNET_WORKERS:-1}
echo "[POOL] launching $workers workers…"

POOL_PIDS=()

trap 'echo "[POOL] Terminating workers..."; kill -- -$$' EXIT

for i in $(seq $workers); do
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