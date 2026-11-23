#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd)
bash "$root/packaging/macos/build-sotto.sh"
cmake -S "$root" -B "$root/build-macos" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=27.0 \
  -DRAGTIME_BUILD_SOTTO=ON \
  -DRAGTIME_MACOS_SOTTO_DIR="$root/build-macos/build-sotto/out" "$@"
cmake --build "$root/build-macos" --parallel 6
