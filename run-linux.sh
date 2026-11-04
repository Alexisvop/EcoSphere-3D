#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'HELP'
Uso: ./run-linux.sh [--build] [--test] [--help]
Sin opciones: abre la plaza; compila si falta el ejecutable.
  --build  Configura y compila sin abrir la aplicacion.
  --test   Configura, compila y ejecuta CTest sin abrir la aplicacion.
  --help   Muestra esta ayuda sin configurar ni compilar.
Requisitos: CMake 3.24+, Ninja, C++17, GLFW 3.5.1 y Python 3.10+.
HELP
}

build=0
test=0
help=0
for option in "$@"; do
    case "$option" in
        --build) build=1 ;;
        --test) test=1 ;;
        --help) help=1 ;;
        *) printf 'Error: opcion desconocida: %s\n' "$option" >&2; exit 2 ;;
    esac
done
if (( help )); then
    usage
    exit 0
fi

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        printf 'Error: falta %s. No se instalaran paquetes automaticamente.\n' "$1" >&2
        exit 127
    fi
}

require_command cmake
require_command ninja
require_command "${CXX:-c++}"
if (( test )); then require_command ctest; fi

root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
cd -- "$root"
build_dir="$root/build/linux-dev"
binary="$build_dir/ecosphere"

# CMake comprueba el compilador C++17, GLFW exacta y Python sin descargar nada.
if cmake --preset linux-dev; then
    :
else
    status=$?
    printf 'Error de configuracion. Revisa el diagnostico anterior y docs/BUILD.md (GLFW 3.5.1, C++17, Python 3.10+).\n' >&2
    exit "$status"
fi
if (( build || test )) || [[ ! -x "$binary" ]]; then
    cmake --build --preset linux-dev --parallel 2
fi
if [[ ! -x "$binary" ]]; then
    printf 'Error: no se genero un ejecutable EcoSphere utilizable.\n' >&2
    exit 1
fi
if (( test )); then
    ctest --preset linux-dev --output-on-failure
    exit 0
fi
if (( build )); then exit 0; fi
if [[ ! -d "$build_dir/shaders" || ! -d "$build_dir/assets" ]]; then
    printf 'Error: faltan shaders o modelos junto al ejecutable. Ejecuta --build.\n' >&2
    exit 1
fi
cd -- "$build_dir"
exec "$binary" --plaza
