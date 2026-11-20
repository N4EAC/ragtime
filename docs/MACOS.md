# macOS

Ragtime uses wxWidgets with the native Cocoa frontend and Core Audio for
voice messages and the Ear. Apple Silicon is the first release target.
Native audio device discovery requires macOS 14.2 or newer.

## Building

Install the Xcode command-line tools, CMake 3.16 or newer, and Git.
Run the following from the repository root. The first configure fetches
wxWidgets 3.2.11 and needs network access:

```sh
cmake -S . -B build-macos \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=14.2 \
  -DRAGTIME_BUILD_WX=ON -DRAGTIME_HAMLIB=OFF \
  -DRAGTIME_PICTURES=OFF -DRAGTIME_RNNOISE=OFF \
  -DRAGTIME_BUILD_SOTTO=OFF
cmake --build build-macos --parallel 4
ctest --test-dir build-macos --output-on-failure --parallel 2
open build-macos/app/Ragtime.app
```

This configuration omits direct Hamlib control, JPEG 2000/WebP/HEIF and
RNNoise. The rigctld connection and built-in picture codecs remain
available. The optional libraries and build switches are described in
[the README](../README.md#libraries). Warnings are errors by default.

An installed Cocoa wxWidgets 3.2.10 or newer can be used by leaving
`RAGTIME_BUILD_WX` off and making its `wx-config` discoverable. To build
an existing checkout, set `RAGTIME_WX_SOURCE` to its directory, including
submodules. wxWidgets 3.2.9 has a native Save dialog regression; the
macOS source build pins a version with the fix.

To install the bundle into a staging directory:

```sh
cmake --install build-macos --prefix "$PWD/dist/macos"
dist/macos/Ragtime.app/Contents/MacOS/Ragtime --version
```

The CMake install alone does not collect third-party shared libraries.
Use static dependencies or package their dylibs before distributing an
app built against system libraries.

## Voice codecs

Ragtime loads sotto at run time. Build the pinned sotto and Codec 2
libraries explicitly, then include them in the app:

```sh
packaging/macos/build-sotto.sh
cmake -S . -B build-macos \
  -DRAGTIME_MACOS_SOTTO_DIR="$PWD/build-macos/build-sotto/out"
cmake --build build-macos --parallel 4
ctest --test-dir build-macos --output-on-failure
cmake --install build-macos --prefix "$PWD/dist/macos"
```

The pin is shared with the Linux and Windows packages in
`packaging/sotto.sh`. The dylibs go in `Contents/Frameworks` and their
licences in `Contents/Resources/licences`. The loader checks
`RAGTIME_SOTTO`, the bundle, the executable directory, then dyld's search
path. The codec architecture must match the application.

`SOTTO_WORK` changes the codec build directory; `SOTTO_SOURCE` uses an
existing checkout without changing it. The optional first script argument
selects the output directory, and `SOTTO_JOBS` controls parallelism.
`RAGTIME_BUILD_SOTTO` is the Linux build helper; use the macOS script here.

## Connecting to Data2G

Run Data2G separately, then enter its host address, KISS port and command
port in Settings > Station. Ragtime neither starts nor bundles the modem.
See [the Data2G interface](DATA2G.md) for the connection details.

Serial-device discovery lists macOS call-out devices (`/dev/cu.*`).
Settings > Rig can connect to rigctld; direct Hamlib control requires
building with Hamlib enabled.

For the existing fake-host bench, follow
[Trying it without Data2G](../README.md#trying-it-without-data2g). The app
executable is `build-macos/app/Ragtime.app/Contents/MacOS/Ragtime`.
Use `--config` with a separate settings file when testing.

## Audio and permissions

Settings > Voice selects the operator's microphone and speakers.
Ragtime excludes the audio devices used by local `data2g-host` and
`data2g-gui` processes, their related/aggregate devices, and remembered
radio devices. Select the radio's input and output in Data2G before
selecting voice devices in Ragtime. A missing explicit device stays
unavailable rather than switching to another device.

macOS asks for microphone permission when Ragtime first opens an input,
including a radio input for the Ear. If denied, enable Ragtime in System
Settings > Privacy & Security > Microphone and retry. Device discovery
and speaker playback do not request microphone access.

View > Ear shows a local input's spectrum and waterfall without playing
it. **Same as data2g-host** follows an unambiguous local modem capture.
With multiple possible inputs, select the radio input explicitly in
**Listen to**. A remote modem's TCP connection does not carry its audio.
The Ear and voice capture independently; Ragtime does not change the
system's default devices or the hardware sample rate.

## Testing

Run CTest with the build commands above. Python 3 enables the application
shutdown regression test. Cocoa widget and application tests require a
logged-in graphical session; fake-host tests need loopback sockets.
The shutdown check uses the standard fake-host ports 8100/8300 and
8110/8310 and skips if those ports are occupied.

Audio tests use synthetic devices and capture, without opening hardware
or requesting microphone access. The packaged codec test, enabled by
`RAGTIME_MACOS_SOTTO_DIR`, relocates a codec bundle and requires the real
encoder/decoder round trips to succeed. Without codecs, the loader tests
still check missing and incompatible libraries.

Before distributing a build, also check microphone permission, voice
recording/playback, the Ear alongside Data2G, device unplug/replug, native
file dialogs, and opening the installed app on a clean Mac. Automated
tests cannot establish radio-device compatibility.
