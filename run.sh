#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ $# -lt 2 ]]; then echo "Uso: bash run.sh rustdds publisher|subscriber [opciones]; bash run.sh opendds publisher|subscriber [opciones]"; exit 2; fi
dds="$1"
role="$2"
shift 2
if [[ "$role" != publisher && "$role" != subscriber ]]; then echo "Rol invalido"; exit 2; fi
case "$dds" in
  rustdds) cargo run --locked -p marine-rustdds --bin "$role" -- "$@" ;;
  opendds)
    source opendds/scripts/environment.sh
    marine_opendds_environment
    binary="${MARINE_DDS_BUILD_DIR:-opendds/build-linux}/$role"
    if [[ ! -x "$binary" ]]; then
      echo "Falta $binary. Ejecuta bash opendds/scripts/build.sh --test; consulta opendds/README.md." >&2
      exit 2
    fi
    dependencies="$(ldd "$binary" 2>&1)" || { echo "$dependencies" >&2; exit 2; }
    if [[ "$dependencies" == *"not found"* ]]; then
      echo "$dependencies" >&2
      echo 'Carga el entorno de OpenDDS: source /ruta/OpenDDS-3.34.0/setenv.sh' >&2
      exit 2
    fi
    exec "$binary" "$@"
    ;;
  *) echo "Implementacion invalida: usa rustdds u opendds" >&2; exit 2 ;;
esac
