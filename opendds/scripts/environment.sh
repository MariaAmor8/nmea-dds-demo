#!/usr/bin/env bash
# Shared by build.sh and the OpenDDS branch of run.sh.
marine_opendds_environment() {
  local root="${1:-${MARINE_DDS_ROOT:-${DDS_ROOT:-$HOME/OpenDDS-3.34.0}}}" file dependencies
  if [[ ! -d "$root" ]]; then
    echo "No existe OpenDDS en $root. Usa --dds-root o MARINE_DDS_ROOT; consulta opendds/README.md." >&2
    return 2
  fi
  root="$(cd "$root" && pwd -P)" || return 2
  if [[ ! -f "$root/VERSION.txt" ]] || ! grep -Eq 'version 3\.34\.0([,[:space:]]|$)' "$root/VERSION.txt"; then
    echo "Se requiere OpenDDS exactamente 3.34.0: $root." >&2
    return 2
  fi
  for file in setenv.sh cmake/OpenDDSConfig.cmake bin/opendds_idl ACE_wrappers/bin/tao_idl lib/libOpenDDS_Dcps.so lib/libOpenDDS_Rtps.so lib/libOpenDDS_Rtps_Udp.so; do
    if [[ ! -f "$root/$file" ]]; then
      echo "Falta $root/$file. OpenDDS puede estar solo descargado o incompleto: ejecuta configure y make; consulta opendds/README.md." >&2
      return 2
    fi
  done
  for file in bin/opendds_idl ACE_wrappers/bin/tao_idl; do
    if [[ ! -x "$root/$file" ]]; then
      echo "Generador IDL no ejecutable: $root/$file." >&2
      return 2
    fi
  done
  # Generated setenv.sh can reference unset variables under bash nounset.
  export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
  source "$root/setenv.sh" || return 2
  if [[ "${DDS_ROOT:-}" != "$root" ]]; then
    echo "setenv.sh apunta a otra instalación (${DDS_ROOT:-sin DDS_ROOT}). Reconfigura OpenDDS en su ubicación actual." >&2
    return 2
  fi
  command -v ldd >/dev/null || { echo 'Falta ldd (libc-bin).' >&2; return 2; }
  for file in bin/opendds_idl ACE_wrappers/bin/tao_idl; do
    dependencies="$(ldd "$root/$file" 2>&1)" || { echo "$dependencies" >&2; return 2; }
    if [[ "$dependencies" == *'not found'* ]]; then
      echo "Dependencias ausentes para $file: $dependencies" >&2
      return 2
    fi
  done
}
