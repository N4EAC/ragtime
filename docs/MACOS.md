# Ragtime macOS

This edition connects to an external Data2G Soft TNC. The independent bundled
modem edition remains at https://github.com/N4EAC/ragtime-data2g.

See the root README for setup, native build dependencies, independent settings
and supported modem protocols. The current tested package is Apple Silicon,
macOS 27 or later. macOS packages are built and tested locally; older hosted CI
runners cannot validate this deployment target.

Run `bash packaging/macos/make-zip.sh` to build the pinned voice codecs, build
Ragtime, run tests, copy runtime libraries and rigctl tools, sign ad hoc, verify
the bundle, and create a ZIP in `dist/`. No Qt or Data2G build is required.

The app is named `Ragtime macOS.app`; it can coexist with `Ragtime.app`.
Signing is currently ad hoc, without Apple notarization. The shipped dylibs
include sotto and Codec 2 and can be replaced with compatible builds; licence
notices are under `Contents/Resources/Licenses`.

Settings > Soft TNC holds the external address, KISS and command ports. The
session data port is command + 1. Audio and PTT configuration belong to the
external modem. Settings > Voice selects voice-message audio devices; View >
Ear captures a local radio input. A network modem connection carries no audio.

macOS requests microphone permission when input capture starts. Enable the
Ragtime macOS entry in System Settings > Privacy & Security > Microphone if
permission was previously denied.
