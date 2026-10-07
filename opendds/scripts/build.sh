#!/usr/bin/env bash
set -euo pipefail
script_dir="$(cd "$(dirname "$0")" && pwd -P)"
source_dir="$(cd "$script_dir/.." && pwd -P)"
repo_dir="$(cd "$source_dir/.." && pwd -P)"
dds_root=''
build_dir="${MARINE_DDS_BUILD_DIR:-$source_dir/build-linux}"
build_type=Debug
jobs=4
run_tests=OFF
integration=OFF
usage() {
  echo 'Uso: bash opendds/scripts/build.sh [--dds-root RUTA] [--build-dir RUTA] [--build-type Debug|Release|RelWithDebInfo|MinSizeRel] [--jobs N] [--test] [--integration-tests]'
  echo 'Rutas relativas de build: raíz del repositorio. --integration-tests incluye CTest completo.'
}
while [[ $# -gt 0 ]]; do
  case "$1" in
    --help|-h) usage; exit 0 ;;
    --test) run_tests=ON; shift ;;
    --integration-tests) integration=ON; run_tests=ON; shift ;;
    --dds-root|--build-dir|--build-type|--jobs)
      [[ $# -ge 2 && -n "$2" && "$2" != --* ]] || { echo "Falta valor para $1" >&2; exit 2; }
      case "$1" in
        --dds-root) dds_root="$2" ;;
        --build-dir) build_dir="$2" ;;
        --build-type) build_type="$2" ;;
        --jobs) jobs="$2" ;;
      esac
      shift 2 ;;
    *) echo "Opción desconocida: $1" >&2; usage >&2; exit 2 ;;
  esac
done
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || { echo '--jobs requiere un entero positivo.' >&2; exit 2; }
case "$build_type" in Debug|Release|RelWithDebInfo|MinSizeRel) ;; *) echo 'Tipo de compilación inválido.' >&2; exit 2 ;; esac
for tool in cmake perl make; do
  command -v "$tool" >/dev/null || { echo "Falta $tool; instala las herramientas Linux descritas en opendds/README.md." >&2; exit 2; }
done
command -v "${CXX:-c++}" >/dev/null || { echo 'Falta el compilador C++ Linux; instala build-essential.' >&2; exit 2; }
if [[ "$run_tests" == ON ]]; then
  command -v ctest >/dev/null || { echo 'Falta ctest.' >&2; exit 2; }
fi
if [[ "$integration" == ON ]]; then
  command -v python3 >/dev/null || { echo 'Falta python3 para integración.' >&2; exit 2; }
fi
source "$script_dir/environment.sh"
marine_opendds_environment "$dds_root"
[[ "$build_dir" == /* ]] || build_dir="$repo_dir/$build_dir"
echo "OpenDDS: $DDS_ROOT; build: $build_dir; $build_type"
cmake -S "$source_dir" -B "$build_dir" -G 'Unix Makefiles' \
  "-DOpenDDS_DIR=$DDS_ROOT/cmake" "-DCMAKE_BUILD_TYPE=$build_type" \
  -DBUILD_TESTING=ON "-DMARINE_INTEGRATION_TESTS=$integration"
cmake --build "$build_dir" --parallel "$jobs"
if [[ "$run_tests" == ON ]]; then
  ctest --test-dir "$build_dir" --output-on-failure
fi
