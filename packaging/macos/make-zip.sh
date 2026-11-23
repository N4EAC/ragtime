#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/../.." && pwd)"
"$root/packaging/macos/build-macos.sh" "$@"
ctest --test-dir "$root/build-macos" --output-on-failure --parallel 2
mkdir -p "$root/dist"
app="$root/dist/Ragtime macOS.app"
if [[ -e "$app" ]]; then mv "$app" "$app.previous.$(date +%s)"; fi
ditto "$root/build-macos/app/Ragtime macOS.app" "$app"
"$root/packaging/macos/deploy.sh" "$app"
python3 "$root/packaging/macos/check-bundle.py" "$app"
ditto -c -k --sequesterRsrc --keepParent "$app" "$root/dist/Ragtime-macOS-$(uname -m).zip"
