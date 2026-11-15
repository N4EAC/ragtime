#!/bin/sh
# Build a multi-resolution macOS app icon from the shared Ragtime PNG.
set -eu

source=$1
output=$2
scratch=$(mktemp -d)
trap 'rm -rf "$scratch"' EXIT
iconset=$scratch/Ragtime.iconset
mkdir "$iconset"

for size in 16 32 128 256 512; do
    /usr/bin/sips -z "$size" "$size" "$source" --out "$iconset/icon_${size}x${size}.png" >/dev/null
    retina=$((size * 2))
    /usr/bin/sips -z "$retina" "$retina" "$source" --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
/usr/bin/iconutil -c icns "$iconset" -o "$output"
