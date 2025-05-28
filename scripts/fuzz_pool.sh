#!/usr/bin/env bash
# Endless driver: keeps launching one fuzz-PnR-equiv cycle after another.

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")"/.. && pwd)"
cd "$PROJECT_ROOT"

workers=${FUZNET_WORKERS:-1}
echo "[POOL] launching $workers workers…"

trap 'echo "[POOL] Terminating workers..."; kill -- -$$' EXIT

for i in $(seq $workers); do
    (
        current_sleep=1
        export WORKER_ID=$i
        
        sleep $((i - 1))
        
        while true; do
            padded_id=$(printf "%0${#workers}d" $i)
            
            if ! ./scripts/fuzzer.sh 2>&1 | sed -u "s/^/[W$padded_id] /"; then
                echo "[W$padded_id] fuzzer.sh failed, retrying in $current_sleep seconds…"
                sleep $current_sleep
                current_sleep=$((current_sleep * 2))
            else
                echo "[W$padded_id] fuzzer.sh completed successfully, resetting sleep time."
                current_sleep=1
            fi

        done
    ) &
done

wait