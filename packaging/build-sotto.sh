#!/bin/bash -e
#
# Builds libsotto at the commit pinned in sotto.sh, with the Codec 2 fork it
# links, and puts in OUT_DIR (default build-sotto/out):
#   libsotto.so.0, libcodec2.so.1.2 (what libsotto needs at run time)
#   licences/sotto.txt (MIT), licences/codec2.txt (LGPL-2.1)
# Needs cmake, make, a C compiler and git. Set SOTTO_SOURCE to a
# checkout to build that instead of cloning.

set -eo pipefail

cd "$(dirname "$(realpath "$0")")/.."
ROOT=$PWD
. packaging/sotto.sh
WORK=${SOTTO_WORK:-$ROOT/build-sotto}
OUT=${1:-$WORK/out}
SRC=${SOTTO_SOURCE:-$WORK/sotto}

if [ -z "$SOTTO_SOURCE" ]; then
    if [ ! -d "$SRC/.git" ]; then
        git clone --quiet "$SOTTO_REPO" "$SRC"
    fi
    git -C "$SRC" fetch --quiet origin "$SOTTO_COMMIT" 2>/dev/null || git -C "$SRC" fetch --quiet origin
    git -C "$SRC" -c advice.detachedHead=false checkout --quiet "$SOTTO_COMMIT"
fi

# Codec 2's library only, from sotto's own fork.
C2="$WORK/codec2"
platform=$(uname -s)
codec2_options=()
if [ "$platform" = Darwin ]; then
    jobs=$(sysctl -n hw.logicalcpu 2>/dev/null || echo 4)
    if [ -n "${MACOSX_DEPLOYMENT_TARGET:-}" ]; then
        codec2_options+=("-DCMAKE_OSX_DEPLOYMENT_TARGET=$MACOSX_DEPLOYMENT_TARGET")
    fi
else
    jobs=$(nproc)
fi
cmake -S "$SRC/codec/codec2" -B "$C2" -DCMAKE_BUILD_TYPE=Release -DUNITTEST=OFF "${codec2_options[@]}" > /dev/null
cmake --build "$C2" --target codec2 -j"$jobs" > /dev/null

make -C "$SRC/c" --no-print-directory clean VARIANT="$SOTTO_VARIANT" > /dev/null
mkdir -p "$OUT/licences"
if [ "$platform" = Darwin ]; then
    # Use the upstream object list and flags. The Makefile's shared target
    # uses an ELF soname, so link the same objects as a Mach-O dylib.
    make -C "$SRC/c" --no-print-directory -j"$jobs" libsotto.a VARIANT="$SOTTO_VARIANT" CODEC2_LIB="$C2/src" > /dev/null
    SOTTO_LIB="$OUT/libsotto.0.dylib"
    C2LIB="$C2/src/libcodec2.1.2.dylib"
    C2NAME=$(basename "$C2LIB")
    "${CC:-cc}" -dynamiclib -Wl,-force_load,"$SRC/c/libsotto.a" -Wl,-headerpad_max_install_names \
        -install_name "@rpath/$(basename "$SOTTO_LIB")" "$C2LIB" -o "$SOTTO_LIB"
    install_name_tool -change "$(otool -D "$C2LIB" | tail -1)" "@loader_path/$C2NAME" "$SOTTO_LIB"
    codesign --force --sign - "$SOTTO_LIB"
else
    make -C "$SRC/c" --no-print-directory libsotto.so VARIANT="$SOTTO_VARIANT" CODEC2_LIB="$C2/src" > /dev/null
    cp -L "$SRC/c/libsotto.so.0" "$OUT/libsotto.so.0"
    C2LIB=$(ldd "$OUT/libsotto.so.0" | awk '/libcodec2/ {print $3}')
fi
cp -L "$C2LIB" "$OUT/$(basename "$C2LIB")"
cp "$SRC/LICENSE" "$OUT/licences/sotto.txt"
cp "$SRC/codec/codec2/COPYING" "$OUT/licences/codec2.txt"
echo "libsotto ($(git -C "$SRC" rev-parse --short HEAD)) and $(basename "$C2LIB") in $OUT"
