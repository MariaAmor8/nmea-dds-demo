#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ $# -lt 2 ]]; then echo "Uso: bash run.sh rustdds publisher|subscriber --local IP --peer IP"; exit 2; fi
dds="$1"
role="$2"
shift 2
if [[ "$dds" != rustdds ]]; then echo "OpenDDS aun no esta implementado."; exit 2; fi
if [[ "$role" != publisher && "$role" != subscriber ]]; then echo "Rol invalido"; exit 2; fi
cargo run --locked -p marine-rustdds --bin "$role" -- "$@"
