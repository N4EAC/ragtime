#!/usr/bin/env bash
set -euo pipefail
app="${1:?Usage: deploy.sh path/to/Ragtime macOS.app}"
for tool in rigctl rigctld; do
    cp "$(brew --prefix hamlib)/bin/$tool" "$app/Contents/MacOS/$tool"
done
licences="$app/Contents/Resources/Licenses"
mkdir -p "$licences"
root="$(cd "$(dirname "$0")/../.." && pwd)"
for pair in "Hamlib:hamlib/COPYING.LIB" "OpenJPEG:openjpeg/LICENSE" "libwebp:webp/COPYING" "libde265:libde265/COPYING" "libheif:libheif/COPYING"; do
    cp "/opt/homebrew/opt/${pair#*:}" "$licences/${pair%%:*}.txt"
done
rnnoise="$root/build-macos/rnnoise-v0.1.1/COPYING"
if [[ ! -f "$rnnoise" ]]; then rnnoise="$root/build-macos/_deps/rnnoise-src/COPYING"; fi
cp "$rnnoise" "$licences/RNNoise.txt"
cp "$root/image/src/coolchic/LICENSE.txt" "$licences/Cool-chic.txt"
cp "$root/packaging/macos/wxWidgets.txt" "$licences/wxWidgets.txt"
python3 "$(dirname "$0")/bundle-libraries.py" "$app"
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
