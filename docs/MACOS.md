# macOS development

The initial port uses the existing wxWidgets frontend with wxOSX/Cocoa.
Build with Apple Clang, CMake 3.16 or newer, and the Xcode command-line
tools. The first build route targets the Mac's native architecture; a
minimum supported OS and Intel/universal distribution are still open.

## Build the desktop app

This configuration builds a static, pinned wxWidgets 3.2.11 from source
and leaves optional Hamlib, image dependencies, RNNoise and sotto out.
Git and network access are needed on the first configure. PipeWire and
automatic Linux sotto building are already off by default on Apple.

```sh
cmake -S . -B build-macos \
  -DCMAKE_BUILD_TYPE=Release -DRAGTIME_BUILD_WX=ON \
  -DRAGTIME_HAMLIB=OFF -DRAGTIME_PICTURES=OFF \
  -DRAGTIME_RNNOISE=OFF -DRAGTIME_BUILD_SOTTO=OFF
cmake --build build-macos --parallel 6
ctest --test-dir build-macos --output-on-failure --parallel 2
open build-macos/app/Ragtime.app
```

To use an existing wxWidgets checkout with its submodules, also pass
`-DRAGTIME_WX_SOURCE=/absolute/path/to/wxWidgets`. To use an installed
Cocoa wxWidgets (3.2.10 or newer) instead, leave `RAGTIME_BUILD_WX` off
and make its `wx-config` discoverable. Warnings remain errors for
Ragtime's sources.

macOS fetches wxWidgets 3.2.11. Version 3.2.9 has a native Save dialog
assertion with a single file filter ([upstream #26148](https://github.com/wxWidgets/wxWidgets/issues/26148)),
fixed in 3.2.10. An existing source checkout must also include that fix.

The widget tests open real Cocoa windows and need a logged-in
graphical session. Fake-host tests need permission to open loopback
sockets. The codec round trip is skipped when no real libsotto is found;
the missing-symbol and bundle-search checks still run.

When Python 3 is available, macOS also registers `ragtime_app_shutdown_test`.
It opens a real app session on its own fake-host bench, quits the app and
checks that a fresh client can call again. Ports 8100/8300 and 8110/8310
must be free; an occupied bench port skips this check before app launch.

Stage the development bundle with:

```sh
cmake --install build-macos --prefix "$PWD/dist/macos"
dist/macos/Ragtime.app/Contents/MacOS/Ragtime --version
```

This is an unsigned development bundle. Signing, notarization and
redistribution of optional third-party dylibs are not implemented. If
optional dependencies or a shared wxWidgets are enabled, installation
does not collect their dylibs. Do not distribute that build as a
self-contained application.

## Directory layout

- `platform/macos/`: native serial-device and codec-path adapters, plus
  the explicit unavailable host-process lookup. The existing audio and
  rig targets select these sources only on Apple.
- `packaging/macos/`: bundle CMake setup, `Info.plist` and Finder/Dock
  icon generation from the existing application artwork.
- `tests/macos/`: native bundle-loader integration checks.
- `app/`, `core/`, `audio/` and `rig/`: shared interfaces, UI and engines.

The bundle follows Apple's [macOS bundle layout](https://developer.apple.com/documentation/bundleresources/placing-content-in-a-bundle):
the executable is in `Contents/MacOS`, artwork in `Contents/Resources`
and optional dylibs in `Contents/Frameworks`.

## Current platform boundaries

Serial enumeration offers sorted `/dev/cu.*` call-out devices, avoiding
duplicate `/dev/tty.*` entries. It does not open or configure the devices.
Direct Hamlib control still needs its own dependency build and hardware
acceptance checks.

The sotto loader checks `RAGTIME_SOTTO` first, then the bundle's
`Contents/Frameworks`, then beside the executable, then dyld's search.
Both `libsotto.0.dylib` and `libsotto.dylib` are recognized. Paths are
resolved from the executable, including symlink launches, independently
of the working directory. A real codec and its dependent libraries must
be built for the same architecture and have suitable dylib install names.
`packaging/build-sotto.sh` remains Linux-only.

Microphone/speaker audio and the Ear remain unavailable until native
backends implement device isolation, authorization and hotplug handling.
Host process lookup returns no PID on macOS; it does not search `/proc`.
Do not infer the radio's audio device from a remote host connection.

## Fake-host bench

Run `build-macos/tests/ragtime_fake_air --files` in a terminal. Launch a
separate app settings file so bench choices stay separate from real
station settings:

```sh
build-macos/app/Ragtime.app/Contents/MacOS/Ragtime --config /tmp/ragtime-macos-bench.conf
```

In Settings > Station, set a callsign, address `127.0.0.1`, KISS port
`8100` and command port `8300`. Connect and use Broadcast, Session and
Files with the fake stations. A second instance can use ports `8110`
and `8310` and a different config file. The bench never uses a radio.
See `CLAUDE.md`, "The bench", for loss, session and reconnect scenarios.

Further acceptance still needs session and file exchange, restart and
reconnect checks, native dialogs, menus and multiple display scales.
See [PORTING.md](PORTING.md) for the original investigation and later
audio/distribution work. GitHub workflows are manual-only; run local
validation before submitting changes.

## Validation of the initial port

Tested on Apple Silicon with macOS 15.8.3, Apple Clang 17 and the static
wxWidgets 3.2.11 configuration above. Release builds keep
`RAGTIME_WERROR=ON`. All 44 registered tests pass (with Python 3), including the bundle
codec search, application version and both Cocoa widget checks.
The real sotto codec round trip remains skipped because no codec is
installed. Other optional features disabled above are not covered.

The installed bundle starts, connects to `ragtime_fake_air`, displays
incoming broadcasts and sends UTF-8 chat through the clipboard (`café —
73`), preserving it in the broadcast log. Its linked dependencies are
Apple system libraries/frameworks in this configuration. Startup testing
found an empty-list focus hang before the first Cocoa size event; the
three HTML list classes now initialize their empty visible ranges, with
GUI checks covering startup and empty/nonempty transitions.

This is development validation on one Mac, not a release or a claim of
complete macOS feature parity. The first-demo checklist in `PORTING.md`
remains open until the remaining interactive scenarios pass.
