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

This is an unsigned development bundle. Signing and notarization are not
implemented. The explicit voice-codec option below collects sotto and
Codec 2 with their licences. Other optional dependencies and a shared
wxWidgets are not collected; those builds are not self-contained.

## The release zip and CI

`packaging/macos/make-zip.sh` builds the zip a release carries,
`dist/Ragtime-<version>-beta-<hash>-macos-<arch>.zip`, on the Mac it runs
on: wxWidgets, Hamlib and the picture libraries (OpenJPEG, libwebp,
libde265, libheif) built statically as the Windows zip has them
(`-DRAGTIME_BUILD_HAMLIB=ON -DRAGTIME_BUILD_IMAGE_LIBS=ON`), RNNoise,
sotto and Codec 2 from `build-sotto.sh` in the bundle, the app signed ad
hoc (`codesign --sign -`) and zipped with `ditto`, with
`packaging/macos/README.txt` and the libraries' licences beside it. Hamlib
needs Homebrew's `autoconf automake libtool`. `MACOSX_DEPLOYMENT_TARGET`
(14.2 by default) is the oldest macOS it runs on.

The full CI run (Actions > Build and test > Run workflow) has a `macos`
job on GitHub's Apple Silicon runner (`macos-15`) that runs the script,
then `ctest --test-dir build-macos`, prints the zip's SHA-256 and keeps
the zip as the run's `ragtime-macos-arm64` artifact, beside the AppImage
and the Windows zip. Pull requests run only the quick Ubuntu job.

The app is not signed with a Developer ID or notarized, so macOS asks
once before it opens it (System Settings > Privacy & Security > Open
Anyway); the zip's README says so. An Intel or universal build is still
open.

## Directory layout

- `platform/macos/`: native serial-device, codec-path and Core Audio voice
  adapters, plus the explicit unavailable host-process lookup. The audio and
  rig targets select these sources only on Apple.
- `packaging/macos/`: bundle CMake setup, `Info.plist` and Finder/Dock
  icon generation from the existing application artwork.
- `tests/macos/`: native bundle, shutdown and audio checks.
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
`packaging/build-sotto.sh` remains Linux-only. The macOS recipe below
builds native dylibs without changing the codec source checkout.

## Build and bundle voice codecs

The codec build is explicit and separate from configuring the application:

```sh
packaging/macos/build-sotto.sh
cmake -S . -B build-macos \
  -DRAGTIME_MACOS_SOTTO_DIR="$PWD/build-macos/build-sotto/out"
cmake --build build-macos --parallel 6
ctest --test-dir build-macos --output-on-failure
cmake --install build-macos --prefix "$PWD/dist/macos"
```

The recipe uses the sotto commit in `packaging/sotto.sh`, its Codec 2
fork and committed v3.8 tables. It needs Apple Clang, CMake and Git, and
network access for the initial checkout. `SOTTO_SOURCE` uses an existing
checkout without fetching or modifying it; `SOTTO_WORK` changes the build
directory. The optional first argument changes the output directory.
`SOTTO_JOBS` controls parallelism (default 4). Builds target the native
architecture and macOS 14.2 by default; Intel/universal builds remain
unverified. Ragtime's `RAGTIME_BUILD_SOTTO` remains Linux-only.

With `RAGTIME_MACOS_SOTTO_DIR` set, CMake requires both dylibs and licence
files. It copies the dylibs into `Contents/Frameworks` and licences into
`Contents/Resources/licences`. Ragtime still loads sotto dynamically;
neither codec is linked into the application. The dylibs use `@rpath`
install names and `@loader_path` search paths, with system-library
dependencies only. These artifacts are development builds, not notarized
releases.

The opt-in `ragtime_packaged_sotto_test` relocates a real codec-test bundle
to a path containing spaces, clears dyld/library overrides, verifies the
loaded sotto path and runs the encoder/decoder checks. It requires the
codec to load rather than silently skipping. The original tests still
cover missing and incompatible libraries when codecs are omitted.

Voice uses Core Audio on macOS 14.2 and later, whose process objects let
Ragtime identify the audio devices used by local `data2g-host` and
`data2g-gui` processes. Discovery does not open audio streams. An older
system, an unresolved radio device or an unreadable ownership inventory
leaves voice unavailable with a status message.

The Ear and voice use native input queues independently. Host-to-command-port
PID lookup still returns no PID on macOS; audio discovery inspects local
audio clients independently of that lookup. Do not infer a remote radio's
audio device from its host connection.

## Native voice audio

Settings > Voice offers the operator's microphones and speakers, leaving
out both halves of every local modem's device and remembered radio devices.
Related devices and aggregate members are excluded together; consequently
an aggregate containing a radio can also exclude its other members.
Selections persist by device UID, independently of transient Core Audio
IDs. A missing explicit selection stays missing. A default that belongs to
the radio selects another allowed device.

Microphone capture opens only when requested. The app's usage description
allows macOS to ask for microphone permission on the first request; denial
is reported in Settings. Enable access in System Settings > Privacy &
Security > Microphone, then retry. The command-line probe does not request
permission without a bundle usage description. Device discovery and
speaker playback do not request microphone permission.

Capture uses mono floats at the device's nominal rate. Playback accepts
mono floats at the clip's rate, using the audio queue's conversion. Both
queues are pinned to the selected UID before starting; Ragtime does not
change system defaults or hardware sample rates. Inventory changes are
polled every 750 ms. Capture follows a new selection or a returning device;
playback cancels if its device disappears or becomes excluded. Cancellation
and shutdown dispose the queues before completing callbacks.

The existing probe can list devices without recording or playing:

```sh
build-macos/tests/ragtime_voice_probe
```

To test output, pass an allowed speaker's exact name from that listing:

```sh
build-macos/tests/ragtime_voice_probe --speaker 'coreaudio:output:BuiltInSpeakerDevice' --play
build-macos/tests/ragtime_voice_probe --speaker 'coreaudio:output:BuiltInSpeakerDevice' --stream
```

The initial audio validation used the built-in speakers on the development
Mac: ordinary and streamed 8 kHz chimes completed, including a stream gap.
A temporary process named `data2g-host` using that output with silence
verified native ownership discovery and exclusion of its microphone twin.
All 49 registered tests pass with the native Ear and bundled voice codecs enabled.
Automated audio tests use synthetic topology, permissions and capture, and
never open hardware or prompt for permission. They cover aggregate and
related-device exclusion, remembered names, missing/refused selections,
stream gaps, attenuation/boost and completion/cancellation during shutdown.

The native Settings microphone test captured and played back built-in
microphone audio. Closing Settings during capture and then quitting the
app completed normally. Microphone access was already authorized on this
Mac; permission waiting, grant/denial transitions and late completion after
stop/destruction are covered with a fake permission adapter. The three
second Settings test pauses its clock while an OS permission prompt is
pending, and the deck disables Push to talk until permission is settled.

Actual first-use permission interaction, physical hotplug and device
changes during active recording/playback still need hardware acceptance.
The real codec round trips pass for 2400, 1200, 600 and 400 bit/s, as well
as playback of the older codec-1 fixture.

The two-instance fake-host bench also passed the complete native voice
path: MACOS1 captured a three-second clip (156 codec-2 frames), monitored
it through sotto, and broadcast the 943-byte `.sot` to MACOS2. The receiver
saved the complete file, byte-for-byte identical to the sender's, and
played it through the built-in speakers to completion. Both instances used
isolated settings and loopback hosts; no real radio was involved.

## Native Ear

View > Ear opens a separate input queue and shows its spectrum and
waterfall. It never plays audio. The Listen to dropdown offers every local
input, including the radio inputs excluded from voice. Selections use the
same stable Core Audio UID names as voice, with independent preferences.

With Same as data2g-host selected, one unambiguous local host capture is
followed automatically. If that host stops recording, the open Ear stays
on its last input; with no previous host input it uses the system default.
Several possible host captures, including several inputs in one embedded
`data2g-gui`, require an explicit selection. Command-port-to-PID lookup is
not implemented on macOS, so the dropdown is how to resolve multiple local
hosts. A missing explicit source remains missing and reopens when its UID
returns. Only local audio is available, even when Ragtime connects to a
remote host.

Capture uses mono floats at the input's nominal rate and three 100 ms
queue buffers. It checks for exclusive ownership and a shareable PCM
format before opening. Ragtime does not set hardware rate, hardware buffer
size, system defaults or exclusive ownership. The queue keeps its input
active while the Ear is open, including after the host stops. Closing the
Ear synchronously stops and disposes that queue; the voice queue has its
own lifetime.

macOS microphone permission also applies to radio audio inputs. The Ear
requests it only when it has an input to open, reports pending/denied
access in its status line, and resumes after approval. Discovery failures
pause capture; selection, device and rate changes are checked every
750 ms, with capture failures and permission revocation checked between
scans. The command-line probe cannot request permission without a bundle
usage description; use the app to grant access first.

To exercise a known input without the window (this records audio):

```sh
build-macos/tests/ragtime_ear_probe \
  --source 'coreaudio:input:BuiltInMicrophoneDevice' --seconds 3
```

The native probe produced live 48 kHz spectrum rows from the development
Mac's built-in microphone. A temporary `data2g-host` process capturing that
same input verified automatic host discovery and simultaneous capture;
the host continued receiving samples after the Ear stopped. The input's
48 kHz rate, 512-frame hardware buffer and unowned exclusive-access state
remained unchanged throughout the check. This uses no actual radio and
does not replace modem/hardware acceptance.

In the Cocoa app, a saved missing source stayed unavailable; selecting the
built-in microphone started the 48 kHz spectrum and waterfall. Settings >
Voice successfully recorded and played back while the Ear stayed open,
and its waterfall continued after the voice test ended. Cmd+Q with Ear
capture active exited normally. Both checks used isolated settings and
the built-in devices only.

The fake-capture tests cover following a moving host, remembering its
input, defaults, explicit missing/reappearing UIDs, rate changes, ambiguous
hosts, discovery/open/capture failure, permission waiting/denial/revocation,
recovery and synchronized shutdown. Native radio-device hotplug,
exclusive-device refusal and modem coexistence still need hardware checks.

## Fake-host bench

For the real Data2G release-host preflight and ESP32 USB mock-radio setup,
see [MACOS_RADIO_BENCH.md](MACOS_RADIO_BENCH.md). Keep the fake-host tests
separate from modem/USB hardware acceptance.

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

The initial demo acceptance covers session/file exchange, restart,
reconnect, native file dialogs and menus on the development Mac. Other
display scales, Intel Macs and release packaging still need validation.
See [PORTING.md](PORTING.md) for the original investigation and later
audio/distribution work.

## Validation of the initial port

Tested on Apple Silicon with macOS 15.8.3, Apple Clang 17 and the static
wxWidgets 3.2.11 configuration above. Release builds keep
`RAGTIME_WERROR=ON`. All 44 registered tests pass (with Python 3),
including the bundle codec search, application version, both Cocoa widget
checks and the real-app session shutdown check.
The initial acceptance run skipped the real sotto round trip because no
codec was installed. The native voice checks above cover the later codec
build. Other optional features disabled above are not covered.

The installed bundle starts, connects to `ragtime_fake_air`, displays
incoming broadcasts and sends UTF-8 chat through the clipboard (`café —
73`), preserving it in the broadcast log. Its linked dependencies are
Apple system libraries/frameworks in this configuration. Startup testing
found an empty-list focus hang before the first Cocoa size event; the
three HTML list classes now initialize their empty visible ranges, with
GUI checks covering startup and empty/nonempty transitions.

This is development validation on one Mac, not a release or a claim of
complete macOS feature parity.

## Desktop acceptance, 2026-11-20

These checks used the installed development bundle, isolated `--config`
profiles, and the fake-host bench. No radio or real modem was involved.
File contents were compared byte for byte, with SHA-256 checked as well.

| Scenario | Observed result |
| --- | --- |
| Session chat and clipboard | UTF-8 `café —` text delivered to W1AW; its response and reaction displayed. |
| Outgoing session file | Native Open dialog selected `café — outbound.txt`; all 2,080 bytes matched the bot's saved copy. |
| Incoming session file | Native Save As stored the 5,734-byte bulletin as `reçu — bulletin.txt`; contents matched the bot's source. |
| History after restart | Session messages, reactions, completed file entries and the kept partial transfer reappeared. |
| Interrupted session transfer | Quit during a 98,304-byte transfer. After restart, **Send the rest to W1AW** opened a new session, reused 13,052 bytes held by W1AW, and completed with an identical SHA-256. |
| Host loss and reconnect | UI showed reconnect backoff, retained a queued UTF-8 broadcast, then delivered it after the host returned. The flight log recorded down/up without restarting the app. |
| Broadcast repair at 25% loss | Recovered a lost file announcement through item parity, requested missing file pieces, and saved the 2,871-byte bulletin with matching contents. Files visibly progressed through **Repairing, 3 pieces missing** to **Saved**. |
| Native UI | Call shortcut, File/View menus, About dialog, file pickers, Cmd+Q and Globus toggle worked. Normal and zoomed layouts were inspected on the same 2x Retina display. Other display scales remain untested. |

The checks exposed two fixes: macOS needs the corrected wxWidgets Save
dialog implementation, and closing Ragtime must explicitly end its active
modem session. Otherwise the modem can retain the old ARQ link after the
client sockets close and reject the next call. The shutdown regression
test exercises this with a real app process and a new fake-host client.

To repeat the session and repair checks, run:

```sh
build-macos/tests/ragtime_fake_air --files --call MACOS1 \
  --rate 2400 --airtime 150 --loss 25
```

Use `MACOS1` in the isolated app profile. Call W1AW, send a message asking
for its bulletin, and exchange files through the native pickers. For
partial recovery, keep the bench running while quitting and restarting
Ragtime; use **Send the rest** on the kept file in Files. For broadcast
repair, leave the session and enable **File > Receive broadcast files
automatically**. Allow the seal windows and repair rounds to finish;
losing the announcement can require a metadata repair before file repair.
Bot source and received files are under the temporary directory's
`ragtime-fake-air-<pid>/W1AW` folder.

For reconnect, stop and restart only the bench, keeping Ragtime open.
Send a broadcast while disconnected, then verify it leaves the queue
after automatic reconnect. The backoff may take about a minute.
