#!/bin/bash
set -euo pipefail
# Native macOS package for the Mac it runs on (arm64 on Apple Silicon,
# x86_64 on Intel), using upstream's release naming and shared dependency
# recipes. Run from any working directory.
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
cd "$ROOT"
ARCH=$(uname -m)
if [ "$(uname -s)" != Darwin ] || { [ "$ARCH" != arm64 ] && [ "$ARCH" != x86_64 ]; }; then
    echo "Build this package on a Mac (Apple Silicon or Intel) using a native shell." >&2
    exit 1
fi
for tool in cmake ctest git python3 autoconf automake glibtoolize; do
    command -v "$tool" >/dev/null || { echo "Missing build tool: $tool" >&2; exit 1; }
done
. packaging/release-name.sh
. packaging/sotto.sh
BUILD_DIR=${BUILD_DIR:-$ROOT/build-macos-release}
mkdir -p "$BUILD_DIR"
BUILD_DIR=$(cd "$BUILD_DIR" && pwd)
JOBS=${JOBS:-4}
export MACOSX_DEPLOYMENT_TARGET=14.2
IDENTITY=${MACOS_SIGN_IDENTITY:--}
if [ -n "${MACOS_NOTARY_PROFILE:-}" ] && [ "$IDENTITY" = - ]; then
    echo "Notarization requires MACOS_SIGN_IDENTITY (a Developer ID Application identity)." >&2
    exit 1
fi

if [ -z "${RAGTIME_MACOS_SOTTO_DIR:-}" ]; then
    RAGTIME_MACOS_SOTTO_DIR="$BUILD_DIR/build-sotto/out"
    SOTTO_WORK="$BUILD_DIR/build-sotto" SOTTO_ARCH="$ARCH" SOTTO_JOBS="$JOBS" \
        packaging/macos/build-sotto.sh "$RAGTIME_MACOS_SOTTO_DIR"
fi
RAGTIME_MACOS_SOTTO_DIR=$(cd "$RAGTIME_MACOS_SOTTO_DIR" && pwd)
if [ "$(cat "$RAGTIME_MACOS_SOTTO_DIR/commit.txt")" != "$SOTTO_COMMIT" ]; then
    echo "Voice codecs must be built at the pin in packaging/sotto.sh." >&2
    exit 1
fi

args=(
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCH"
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET"
    -DRAGTIME_APP=ON -DRAGTIME_TESTS=ON -DRAGTIME_WERROR=ON
    -DRAGTIME_BUILD_WX=ON -DRAGTIME_HAMLIB=ON -DRAGTIME_BUILD_HAMLIB=ON
    -DRAGTIME_EMBED_DATA2G=OFF
    -DRAGTIME_PICTURES=ON -DRAGTIME_HEIF=ON -DRAGTIME_BUILD_IMAGE_LIBS=ON
    -DRAGTIME_RNNOISE=ON -DRAGTIME_PIPEWIRE=OFF -DRAGTIME_BUILD_SOTTO=OFF
    -DRAGTIME_MACOS_SOTTO_DIR="$RAGTIME_MACOS_SOTTO_DIR"
    -DRAGTIME_SOTTO_TEST_LIB="$RAGTIME_MACOS_SOTTO_DIR/libsotto.0.dylib"
)
if [ -n "${RAGTIME_WX_SOURCE:-}" ]; then args+=(-DRAGTIME_WX_SOURCE="$RAGTIME_WX_SOURCE"); fi
if [ -n "${RAGTIME_HAMLIB_SOURCE:-}" ]; then args+=(-DRAGTIME_HAMLIB_SOURCE="$RAGTIME_HAMLIB_SOURCE"); fi
cmake -S . -B "$BUILD_DIR" "${args[@]}"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
ctest --test-dir "$BUILD_DIR" --output-on-failure --parallel 2

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
cmake --install "$BUILD_DIR" --prefix "$WORK"
APP="$WORK/Ragtime.app"
LICENCES="$APP/Contents/Resources/licences"
mkdir -p "$LICENCES"
cp LICENSE "$APP/Contents/Resources/LICENSE.txt"
cp packaging/macos/README.txt "$WORK/README.txt"
cp LICENSE "$WORK/LICENSE.txt"
WX_SOURCE=${RAGTIME_WX_SOURCE:-$BUILD_DIR/_deps/wxwidgets-src}
DEPS="$BUILD_DIR/_deps"
for pair in \
    "wxWidgets:$WX_SOURCE/docs/licence.txt" \
    "Hamlib:$DEPS/hamlib/src/hamlib_build/COPYING.LIB" \
    "OpenJPEG:$DEPS/openjpeg/src/openjpeg_build/LICENSE" \
    "libwebp:$DEPS/webp/src/webp_build/COPYING" \
    "libde265:$DEPS/de265/src/de265_build/COPYING" \
    "libheif:$DEPS/heif/src/heif_build/COPYING" \
    "RNNoise:$DEPS/rnnoise-src/COPYING" \
    "Cool-chic:$ROOT/image/src/coolchic/LICENSE.txt"; do
    cp "${pair#*:}" "$LICENCES/${pair%%:*}.txt"
done

# Sign nested code first; --deep is for verification, not signing.
sign_args=(--force --sign "$IDENTITY")
if [ "$IDENTITY" != - ]; then sign_args+=(--options runtime --timestamp); fi
for library in "$APP/Contents/Frameworks/"*.dylib; do
    codesign "${sign_args[@]}" "$library"
done
codesign "${sign_args[@]}" --entitlements packaging/macos/entitlements.plist "$APP"
python3 packaging/macos/check-bundle.py "$APP"
"$APP/Contents/MacOS/Ragtime" --version

mkdir -p dist
OUT="$ROOT/dist/Ragtime-$RAGTIME_RELEASE-macos-$ARCH.zip"
if [ -n "${MACOS_NOTARY_PROFILE:-}" ]; then
    ditto -c -k --keepParent "$APP" "$WORK/notarize.zip"
    xcrun notarytool submit "$WORK/notarize.zip" \
        --keychain-profile "$MACOS_NOTARY_PROFILE" --wait
    xcrun stapler staple "$APP"
    xcrun stapler validate "$APP"
    rm "$WORK/notarize.zip"
fi
ditto -c -k "$WORK" "$OUT.tmp"
mv "$OUT.tmp" "$OUT"
(cd dist && shasum -a 256 "$(basename "$OUT")" > "$(basename "$OUT").sha256")
if [ "$IDENTITY" = - ]; then
    echo "Ad-hoc signed package for local testing; use Developer ID and notarization for public distribution."
fi
echo "Done: $OUT"
