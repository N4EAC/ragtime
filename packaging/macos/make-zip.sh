#!/bin/bash -e
#
# Builds Ragtime for macOS on this Mac's architecture and zips the app as
# dist/Ragtime-<version>-beta-<hash>-macos-<arch>.zip: Ragtime.app (with
# wxWidgets, Hamlib and the picture codecs linked in, and libsotto and
# libcodec2 for voice in Contents/Frameworks with their licences),
# README.txt, LICENSE.txt and the libraries' licences in licences/.
# There is no installer, and the app is not signed by a developer: it is
# signed ad hoc, so macOS asks once before it opens (README.txt says how).
#
# Needs the Xcode command-line tools, CMake, git, and autoconf, automake
# and libtool for Hamlib (Homebrew's). The first build clones wxWidgets,
# Hamlib, OpenJPEG, libwebp, libde265, libheif, RNNoise and sotto into
# build-macos (set RAGTIME_WX_SOURCE or RAGTIME_HAMLIB_SOURCE to a checkout
# to use that instead) and takes a while. Set BUILD_DIR to use another
# build tree. After it, `ctest --test-dir build-macos` runs the tests,
# sotto's round trip in a bundle included.
#
# MACOSX_DEPLOYMENT_TARGET (14.2 by default: voice needs Core Audio's
# process objects) sets the oldest macOS it runs on.

cd "$(dirname "$0")/../.."
ROOT=$PWD
. packaging/release-name.sh
ARCH=$(uname -m)
NAME="Ragtime-$RAGTIME_RELEASE-macos-$ARCH"
BUILD_DIR=${BUILD_DIR:-$ROOT/build-macos}
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-14.2}
JOBS=$(sysctl -n hw.ncpu)

# libsotto and libcodec2, built once per pinned commit.
. packaging/sotto.sh
SOTTO_OUT="$BUILD_DIR/build-sotto/out"
if [ ! -f "$SOTTO_OUT/libsotto.0.dylib" ] \
    || [ "$(cat "$SOTTO_OUT/commit.txt" 2>/dev/null)" != "$SOTTO_COMMIT" ]; then
    rm -rf "$SOTTO_OUT"
    SOTTO_WORK="$BUILD_DIR/build-sotto" SOTTO_JOBS="$JOBS" packaging/macos/build-sotto.sh "$SOTTO_OUT"
fi

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$MACOSX_DEPLOYMENT_TARGET" \
    -DRAGTIME_BUILD_WX=ON ${RAGTIME_WX_SOURCE:+-DRAGTIME_WX_SOURCE="$RAGTIME_WX_SOURCE"} \
    -DRAGTIME_BUILD_HAMLIB=ON ${RAGTIME_HAMLIB_SOURCE:+-DRAGTIME_HAMLIB_SOURCE="$RAGTIME_HAMLIB_SOURCE"} \
    -DRAGTIME_BUILD_IMAGE_LIBS=ON -DRAGTIME_BUILD_SOTTO=OFF \
    -DRAGTIME_MACOS_SOTTO_DIR="$SOTTO_OUT"
cmake --build "$BUILD_DIR" --parallel "$JOBS"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir "$WORK/$NAME"
cmake --install "$BUILD_DIR" --prefix "$WORK/$NAME"
APP="$WORK/$NAME/Ragtime.app"
strip -x "$APP/Contents/MacOS/Ragtime"
# Apple Silicon runs only signed code. Sign the whole bundle ad hoc, the
# dylibs first, so its seal covers what is in it now.
codesign --force --sign - "$APP"/Contents/Frameworks/*.dylib
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"
"$APP/Contents/MacOS/Ragtime" --version
cp packaging/macos/README.txt "$WORK/$NAME/README.txt"
cp LICENSE "$WORK/$NAME/LICENSE.txt"
# The libraries linked in, each under its own licence (README.txt says
# where their source and Ragtime's are, to rebuild it with others).
mkdir "$WORK/$NAME/licences"
DEPS="$BUILD_DIR/_deps"
for pair in \
    "wxWidgets:$DEPS/wxwidgets-src/docs/licence.txt" \
    "Hamlib:$DEPS/hamlib/src/hamlib_build/COPYING.LIB" \
    "OpenJPEG:$DEPS/openjpeg/src/openjpeg_build/LICENSE" \
    "libwebp:$DEPS/webp/src/webp_build/COPYING" \
    "libde265:$DEPS/de265/src/de265_build/COPYING" \
    "libheif:$DEPS/heif/src/heif_build/COPYING" \
    "RNNoise:$DEPS/rnnoise-src/COPYING"; do
    cp "${pair#*:}" "$WORK/$NAME/licences/${pair%%:*}.txt"
done

mkdir -p dist
rm -f "dist/$NAME.zip"
# ditto keeps the bundle's links and signature as Finder's own zip does.
(cd "$WORK" && ditto -c -k --keepParent "$NAME" "$ROOT/dist/$NAME.zip")
echo "dist/$NAME.zip"
