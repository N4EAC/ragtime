#!/bin/bash
set -euo pipefail
# Native development codecs. No source-tree objects, GNU utilities or
# Python generators; uses the committed tables at packaging/sotto.sh's pin.
# SOTTO_SOURCE uses an existing checkout without changing it.
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
. "$ROOT/packaging/sotto.sh"
if [ "$(uname -s)" != Darwin ]; then
    echo "This recipe builds macOS dylibs; use packaging/build-sotto.sh on Linux" >&2
    exit 1
fi
WORK=${SOTTO_WORK:-$ROOT/build-macos/build-sotto}
mkdir -p "$WORK"
WORK=$(cd "$WORK" && pwd)
SRC=${SOTTO_SOURCE:-$WORK/sotto}
if [ -z "${SOTTO_SOURCE:-}" ]; then
    if [ ! -d "$SRC/.git" ]; then
        git clone --quiet "$SOTTO_REPO" "$SRC"
    fi
    git -C "$SRC" fetch --quiet origin "$SOTTO_COMMIT"
    git -C "$SRC" -c advice.detachedHead=false checkout --quiet "$SOTTO_COMMIT"
fi
SRC=$(cd "$SRC" && pwd)
OUT=${1:-$WORK/out}
mkdir -p "$OUT"
OUT=$(cd "$OUT" && pwd)
cmake -S "$ROOT/packaging/macos/sotto" -B "$WORK/build" \
    -DSOTTO_SOURCE="$SRC" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES="${SOTTO_ARCH:-$(uname -m)}" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-14.2}" \
    -DCMAKE_INSTALL_PREFIX="$OUT"
cmake --build "$WORK/build" --parallel "${SOTTO_JOBS:-4}"
cmake --install "$WORK/build"
git -C "$SRC" rev-parse HEAD > "$OUT/commit.txt"
echo "macOS voice codecs and licences in $OUT"
