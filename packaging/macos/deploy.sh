#!/usr/bin/env bash
# Deploy a staging copy of Ragtime.app and sign it for local use.
set -euo pipefail

app="${1:?Usage: packaging/macos/deploy.sh path/to/staged/Ragtime.app}"
test -x "$app/Contents/MacOS/Ragtime"
qtbase="$(brew --prefix qtbase)"
wxlib="$(brew --prefix wxwidgets)/lib"
frameworks="$app/Contents/Frameworks"

tools=()
if otool -L "$app/Contents/MacOS/Ragtime" | grep libhamlib >/dev/null; then
    for tool in rigctl rigctld; do
        cp "$(brew --prefix hamlib)/bin/$tool" "$app/Contents/MacOS/$tool"
        chmod u+w "$app/Contents/MacOS/$tool"
        tools+=("-executable=$app/Contents/MacOS/$tool")
    done
fi
"$qtbase/bin/macdeployqt" "$app" -no-plugins -always-overwrite ${tools[@]+"${tools[@]}"}

# wx-config links the short dylib names, while wx's own libraries may
# refer to the fully versioned names. Load only one copy of each library.
for source in "$wxlib"/libwx*.dylib; do
    alias="${source##*/}"
    resolved="$(realpath "$source")"
    library="${resolved##*/}"
    if [[ "$alias" != "$library" && -f "$frameworks/$alias" && -f "$frameworks/$library" ]]; then
        ln -sfn "$library" "$frameworks/$alias"
    fi
done

codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"
