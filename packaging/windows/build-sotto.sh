#!/bin/bash -e
#
# Cross-builds sotto for 64-bit Windows at the commit pinned in
# packaging/sotto.sh, with the Codec 2 fork it links, and puts in OUT_DIR
# (default build-win/build-sotto/out):
#   sotto.dll, libcodec2.dll (what sotto.dll needs at run time)
#   licences/sotto.txt (MIT), licences/codec2.txt (LGPL-2.1)
#   commit.txt (the sotto commit built)
# make-zip.sh runs it and puts them beside ragtime.exe, which loads
# sotto.dll at run time and never links it (LGPL through Codec 2).
#
# Needs the posix-thread MinGW-w64 GCC (gcc-mingw-w64-x86-64-posix), a
# native C compiler (Codec 2 builds a code generator for the build
# machine), cmake, make and git. Set SOTTO_SOURCE to a checkout to build
# that instead of cloning; its c/ folder's objects are rebuilt.

cd "$(dirname "$(realpath "$0")")/../.."
ROOT=$PWD
. packaging/sotto.sh
WORK=${SOTTO_WORK:-$ROOT/build-win/build-sotto}
OUT=${1:-$WORK/out}
SRC=${SOTTO_SOURCE:-$WORK/sotto}
HOST=x86_64-w64-mingw32
CC_WIN=$HOST-gcc-posix

if [ -z "$SOTTO_SOURCE" ]; then
    if [ ! -d "$SRC/.git" ]; then
        git clone --quiet "$SOTTO_REPO" "$SRC"
    fi
    git -C "$SRC" fetch --quiet origin "$SOTTO_COMMIT" 2>/dev/null || git -C "$SRC" fetch --quiet origin
    git -C "$SRC" -c advice.detachedHead=false checkout --quiet "$SOTTO_COMMIT"
fi

# Codec 2's library only, from sotto's own fork, as libcodec2.dll. The
# GCC runtime goes inside it, so it needs no libgcc_s_seh-1.dll.
C2="$WORK/codec2"
cmake -S "$SRC/codec/codec2" -B "$C2" -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/Toolchain-mingw64.cmake" \
    -DCMAKE_BUILD_TYPE=Release -DUNITTEST=OFF -DCMAKE_SHARED_LINKER_FLAGS=-static-libgcc > /dev/null
cmake --build "$C2" --target codec2 -j"$(nproc)" > /dev/null

# sotto's objects as its Makefile compiles them (the variant's flags with
# them), then one DLL from them. sotto.h marks the API dllexport when
# SOTTO_BUILD is defined, which sotto's sources do, so only the API is
# exported.
make -C "$SRC/c" --no-print-directory clean VARIANT="$SOTTO_VARIANT" > /dev/null
make -C "$SRC/c" --no-print-directory libsotto.a VARIANT="$SOTTO_VARIANT" CODEC2_LIB="$C2/src" \
    CC="$CC_WIN" AR="$HOST-ar" > /dev/null
mkdir -p "$OUT/licences"
"$CC_WIN" -shared -static-libgcc -o "$OUT/sotto.dll" \
    -Wl,--whole-archive "$SRC/c/libsotto.a" -Wl,--no-whole-archive -L"$C2/src" -lcodec2 -lm
# Leave the checkout's c/ folder clean for a native build after this one.
make -C "$SRC/c" --no-print-directory clean VARIANT="$SOTTO_VARIANT" > /dev/null
cp "$C2/src/libcodec2.dll" "$OUT/"
"$HOST-strip" "$OUT/sotto.dll" "$OUT/libcodec2.dll"

# Neither may need a MinGW DLL that isn't in the zip.
for dll in sotto.dll libcodec2.dll; do
    extra=$("$HOST-objdump" -p "$OUT/$dll" | awk '/DLL Name:/ {print $3}' \
        | grep -v -i -x -e KERNEL32.dll -e msvcrt.dll -e libcodec2.dll || true)
    if [ -n "$extra" ]; then
        echo "$dll needs DLLs the zip doesn't carry: $extra" >&2
        exit 1
    fi
done

cp "$SRC/LICENSE" "$OUT/licences/sotto.txt"
cp "$SRC/codec/codec2/COPYING" "$OUT/licences/codec2.txt"
git -C "$SRC" rev-parse HEAD > "$OUT/commit.txt"
echo "sotto.dll ($(git -C "$SRC" rev-parse --short HEAD)) and libcodec2.dll in $OUT"
