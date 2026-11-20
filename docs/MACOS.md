# Ragtime on macOS

The macOS port is Tim Vetter's work. It is the same wxWidgets app as on
Linux and Windows, built against wxWidgets' Cocoa toolkit, with the Mac's
own code for audio, serial ports and finding sotto. Everything works as on
the other platforms: chat, sessions, files, pictures, voice, the Ear,
Globus, the gearshift and rig control (Hamlib, or the dial from rigctld).

It needs macOS 14.2 or later, on Apple Silicon or Intel. Voice and the
Ear use Core Audio's process objects, which came in 14.2, to tell which
audio devices data2g-host is using.

## Building

Install the Xcode command-line tools (`xcode-select --install`), then
CMake, and the tools Hamlib's build needs, with Homebrew:

    brew install cmake autoconf automake libtool

The release zip is one command:

    packaging/macos/make-zip.sh    # dist/Ragtime-<version>-...-macos-<arch>.zip

It builds for the Mac it runs on (`arm64` or `x86_64`). The first run
clones and builds wxWidgets, Hamlib, the picture libraries, RNNoise and
sotto into `build-macos`, so it takes a while; later runs only rebuild
what changed. After it, `ctest --test-dir build-macos --output-on-failure`
runs the tests.

For a quicker build while working on the code, leave out the libraries
you don't need:

    cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release -DRAGTIME_BUILD_WX=ON \
        -DRAGTIME_HAMLIB=OFF -DRAGTIME_PICTURES=OFF
    cmake --build build-macos --parallel
    ctest --test-dir build-macos --output-on-failure
    open build-macos/app/Ragtime.app

The build options are those of the other platforms (README, "Building"):
`-DRAGTIME_BUILD_HAMLIB=ON` and `-DRAGTIME_BUILD_IMAGE_LIBS=ON` build
Hamlib and the picture libraries statically, as the release zip does, or
leave them `OFF` to use Homebrew's (`brew install pkgconf hamlib openjpeg
webp libheif`), which ties the build to that Mac. `RAGTIME_WX_SOURCE` and
`RAGTIME_HAMLIB_SOURCE` name checkouts to build instead of cloning. An
installed Cocoa wxWidgets works too (leave `RAGTIME_BUILD_WX` off), if it
is 3.2.10 or newer.

The Mac builds wxWidgets 3.2.11, not the 3.2.9 that Windows uses: 3.2.9's
native Save dialog asserts when given a single file filter
([wxWidgets #26148](https://github.com/wxWidgets/wxWidgets/issues/26148)),
fixed in 3.2.10. Hamlib's command-line tools don't compile against the
current macOS SDK, so the Mac build makes only the library
(`cmake/HamlibNoTools.mk`). `MACOSX_DEPLOYMENT_TARGET` (14.2 by default)
sets the oldest macOS the build runs on.

### Voice codecs

sotto and its Codec 2 are built as dylibs by
`packaging/macos/build-sotto.sh`, at the commit pinned in
`packaging/sotto.sh`, into `build-macos/build-sotto/out`. `make-zip.sh`
runs it once per pin. To bundle them in a build of your own, pass the
folder:

    packaging/macos/build-sotto.sh
    cmake -S . -B build-macos -DRAGTIME_MACOS_SOTTO_DIR="$PWD/build-macos/build-sotto/out"

CMake then copies both dylibs into `Ragtime.app/Contents/Frameworks` and
their licences into `Contents/Resources/licences`. Ragtime still loads
sotto at run time and never links it. `SOTTO_SOURCE` names a sotto
checkout to use instead of cloning one, and `SOTTO_WORK` another build
folder. (`RAGTIME_BUILD_SOTTO`, which builds libsotto while configuring,
is Linux only.)

Ragtime looks for sotto in `$RAGTIME_SOTTO`, then in the bundle's
`Contents/Frameworks`, then beside the program, then wherever dyld looks;
it takes `libsotto.0.dylib` or `libsotto.dylib`. Paths are found from the
program itself, so the app can be moved or started through a link.

## Testing

`ctest` runs the same tests as on Linux, plus the Mac's own in
`tests/macos/`: Core Audio devices, voice and the Ear against a pretend
audio system (they never open real devices or ask for the microphone),
and sotto found inside a bundle. With the voice codecs bundled,
`ragtime_packaged_sotto_test` moves a bundle to a folder whose name has
spaces and checks that sotto still loads from it and codes a clip.

A few tests need more than a terminal:

- `ragtime_chat_list_test` and `ragtime_recent_work_pane_test` open real
  windows, so they need a logged-in desktop session.
- `ragtime_app_shutdown_test` (registered when Python 3 is found) starts
  the app against its own fake hosts, quits it, and checks that another
  client can call again. It needs ports 8100, 8300, 8110 and 8310 free,
  and is skipped when one is taken.

The bench is the same as on Linux (CLAUDE.md, "The bench"):
`build-macos/tests/ragtime_fake_air` runs two fake data2g-hosts, and
`build-macos/app/Ragtime.app/Contents/MacOS/Ragtime --config bench.conf`
runs a Ragtime with settings of its own, to point at one of them in
Settings > Station.

To list the Mac's audio devices as voice and the Ear see them, with
nothing recorded or played:

    build-macos/tests/ragtime_voice_probe

`--speaker NAME --play` plays a test chime on one of them, and
`ragtime_ear_probe --source NAME --seconds 3` prints what the Ear hears
from an input. The names are Core Audio's, such as
`coreaudio:input:BuiltInMicrophoneDevice`. The probes can't ask for the
microphone (only the app can), so let the app have it first.

## The release zip

`Ragtime-<version>-beta-<hash>-macos-arm64.zip` (Apple Silicon) and
`...-macos-x86_64.zip` (Intel) each hold:

- `Ragtime.app`, with wxWidgets, Hamlib, the picture libraries and
  RNNoise linked in, and `libsotto.0.dylib` and its Codec 2 in
  `Contents/Frameworks`;
- `README.txt` (`packaging/macos/README.txt`), `LICENSE.txt`, and the
  libraries' licences in `licences/`.

The app is signed ad hoc (`codesign --sign -`), not with an Apple
Developer ID, and is not notarized. macOS therefore stops it the first
time it opens; System Settings > Privacy & Security > Open Anyway lets it
run, and the zip's README says so.

The full CI run (Actions > Build and test > Run workflow) builds both on
GitHub's Mac runners (`macos-15` and `macos-15-intel`): `make-zip.sh`,
then `ctest`, then the zip's SHA-256 in the log and the run summary. The
zips are kept as the run's `ragtime-macos-arm64` and
`ragtime-macos-x86_64` artifacts, beside the AppImage and the Windows zip.
Pull requests run only the quick Ubuntu job.

## What is different on the Mac

**The app bundle.** `packaging/macos/Bundle.cmake` makes `Ragtime.app`:
the program in `Contents/MacOS`, the icon (made from `app/res/ragtime.png`
by `make-icon.sh`) in `Contents/Resources`, and `Info.plist` from
`Info.plist.in`, with the words macOS shows when it asks for the
microphone. The bundle identifier is `io.github.baitisj.ragtime`.
Settings are in `~/Library/Preferences/Ragtime Preferences`, and
Ragtime's own files in `~/Library/Application Support/Ragtime`.

**Voice.** `platform/macos/CoreAudioVoice.mm`. Settings > Voice offers
the operator's microphones and speakers and leaves out every device a
local `data2g-host` or `data2g-gui` uses, both halves of it, found
through Core Audio's process objects. Devices joined to one of those
(an aggregate device, or the two halves of one card) go with it, as do
the radio devices remembered from before. Choices are kept by device UID,
so they survive a replug. A chosen device that is missing stays missing
rather than falling back to another, and a default that is the radio's
moves to another device. The microphone opens only while recording;
macOS asks for it the first time, and Settings and the deck say when
permission is still pending or was refused. Ragtime never changes the
system defaults or a device's sample rate. Devices are checked every
750 ms.

**The Ear.** `platform/macos/CoreAudioEar.mm`. It records on a queue of
its own (three 100 ms buffers at the input's own rate) and never plays.
Listen to offers every input, the radio's included. With Same as
data2g-host it follows a local host's input when there is exactly one;
with several (two hosts, or a data2g-gui recording from two inputs),
pick one. Ragtime can't tell a host's process from its command port on
the Mac, and only local audio can be heard, even when Ragtime talks to a
host on another computer. It never takes a device for itself or changes
its rate or buffer size. The microphone permission covers radio inputs
too, so the Ear asks for it when it first has something to open.

**Serial ports.** `platform/macos/SerialPorts.cpp` offers the `/dev/cu.*`
devices (the call-out ones, not their `/dev/tty.*` twins) in Settings >
Rig.

**Leaving on quit.** Ragtime leaves its session when it quits, before
its sockets close, so data2g-host doesn't keep the old link and refuse
the next call. (This came from the port, and helps every platform.)

## Not done yet

- Signing with a Developer ID and notarizing, so that macOS opens the
  app without asking.
- Trying Hamlib with a real radio on a Mac, and the first-time microphone
  prompt, device hotplug and data2g-host sharing a real radio's card.
- A single universal app in place of two zips.
