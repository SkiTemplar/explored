#!/usr/bin/env bash
# Compila y ejecuta los tests del host. Uso: Tools/HostTests/run.sh [filtro]
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="${HOST_TESTS_BUILD_DIR:-$HERE/build${HOST_TESTS_SANITIZE:+-san}}"
SAN="${HOST_TESTS_SANITIZE:-OFF}"
cmake -S "$HERE" -B "$BUILD" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DHOST_TESTS_SANITIZE="$SAN" >/dev/null
cmake --build "$BUILD" -j"$(nproc)" 2>&1 | grep -Ev "^\[|^-- |Built target|Consolidate|Scanning" || true
"$BUILD/explored_host_tests" "$@"
