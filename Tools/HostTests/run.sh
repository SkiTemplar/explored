#!/usr/bin/env bash
# Compila y ejecuta los tests del host. Uso: Tools/HostTests/run.sh [filtro]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${HOST_TESTS_BUILD_DIR:-$HERE/build${HOST_TESTS_SANITIZE:+-san}}"
SAN="${HOST_TESTS_SANITIZE:-OFF}"
cmake -S "$HERE" -B "$BUILD" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHOST_TESTS_SANITIZE="$SAN" >/dev/null
# Si la compilación falla no se ejecuta el binario anterior (daría un verde falso).
if ! cmake --build "$BUILD" -j"$(nproc)" >"$BUILD/build.log" 2>&1; then
	grep -Ev "^\[|^-- |Built target|Consolidate|Scanning" "$BUILD/build.log" || true
	echo "Error de compilación de los tests del host" >&2
	exit 1
fi
grep -Ev "^\[|^-- |Built target|Consolidate|Scanning" "$BUILD/build.log" || true
"$BUILD/explored_host_tests" "$@"
