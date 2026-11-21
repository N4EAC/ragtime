#!/bin/sh -e
# Renders Ragtime's icon, app/res/ragtime.svg (drawn by make_leaf_svg.py),
# into the files the builds use:
#   app/res/ragtime.png  256 px, for the AppImage, the desktop file and the
#                        window icon on Linux
#   app/res/ragtime.ico  16 to 256 px, for ragtime.exe and its windows
#   app/res/ragtime.icns on macOS, for Finder and the Dock
# Run it after changing the SVG and commit the rendered icons.
# Needs rsvg-convert (librsvg2-bin) and ImageMagick.

cd "$(dirname "$0")/../../app/res"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# The macOS fork has dedicated high-resolution glass Earth/binary artwork.
if [ "$(uname -s)" = Darwin ] && [ -f ragtime-macos.png ]; then
    sips -z 256 256 ragtime-macos.png --out ragtime.png >/dev/null
    mkdir "$tmp/ragtime.iconset"
    for size in 16 32 128 256 512; do
        sips -z "$size" "$size" ragtime-macos.png --out "$tmp/ragtime.iconset/icon_${size}x${size}.png" >/dev/null
        retina=$((size * 2))
        sips -z "$retina" "$retina" ragtime-macos.png --out "$tmp/ragtime.iconset/icon_${size}x${size}@2x.png" >/dev/null
    done
    iconutil -c icns "$tmp/ragtime.iconset" -o ragtime.icns
    exit 0
fi

rsvg-convert -w 256 -h 256 ragtime.svg -o ragtime.png
for size in 16 20 24 32 40 48 64 128 256; do
    rsvg-convert -w $size -h $size ragtime.svg -o "$tmp/$size.png"
done
convert "$tmp/16.png" "$tmp/20.png" "$tmp/24.png" "$tmp/32.png" "$tmp/40.png" \
    "$tmp/48.png" "$tmp/64.png" "$tmp/128.png" "$tmp/256.png" ragtime.ico

if [ "$(uname -s)" = Darwin ]; then
    /bin/sh ../../tools/icon/make_macos_icon.sh ragtime.png ragtime.icns
fi
