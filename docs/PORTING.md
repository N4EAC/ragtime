# macOS and Android port investigation

Started 2026-11-19 for the `finitor/ragtime` checkout, against
`60225d4e7bc91c48d461d705ddcd5ab7c7990b39` (0.3.1-beta).
Working branch: `codex/macos-android-investigation`.

## Goal and first recommendation

Establish a build route, a small working client, and a measured list of
remaining work for each platform before committing to full feature parity.
Keep the existing C++ protocol engines and wire formats. Start macOS with
the wxWidgets Cocoa frontend; investigate Android as a separate mobile
frontend over the same C++ core.

The first Android scope is a foreground client connecting to an operator's
separately running `data2g-host` over a local network. Running the modem on
the phone is a separate investigation. Ragtime continues to neither bundle
nor launch the modem. Use the existing fake-host/replay bench for tests,
not a real radio. Preserve the operator's explicit session and rig-control
choices described in `CLAUDE.md` and `docs/PLAN.md`.

This is a kickoff and feasibility record, not a claim that either app port
is complete. GitHub Issues is disabled for this repository (the kickoff
issue creation returned HTTP 410); the checklist below is the initial
project tracker.

## Evidence from the checkout

| Area | Evidence | Porting implication |
| --- | --- | --- |
| Shared engines | `core/CMakeLists.txt`, `core/include/ragtime/` | Socket-free C++17 engines for protocol, chat, sessions, files, persistence, spectrum and voice processing are the reuse boundary. Core still includes a Linux `/proc` helper; portable compilation does not make that helper work on other systems. |
| Desktop shell | `app/CMakeLists.txt` | wxWidgets 3.2; GTK dependency is already excluded on Apple. There is no `MACOSX_BUNDLE` target or macOS packaging directory. |
| Transport | `app/src/HostConnection.{h,cpp}`, `RigConnection.{h,cpp}` | Socket and timer adapters use wxWidgets and live outside core. Android must supply their behavior, including three host streams, partial writes and reconnect. |
| Settings | `app/src/Prefs.cpp` | wxConfig and wxStandardPaths are desktop adapters. Android needs preferences and app-private paths of its own. |
| Audio | `audio/CMakeLists.txt`, `audio/include/ragtime/audio/` | PipeWire and WASAPI implementations exist. macOS selects `NoEar` and `NoVoice`; Android should explicitly disable PipeWire, whose default currently follows the generic non-Apple/non-Windows branch. |
| Voice codec | `audio/src/Sotto.cpp`, `packaging/build-sotto.sh` | Every non-Windows platform currently uses `libsotto.so.0` and `/proc/self/exe`. macOS needs executable/bundle paths and dylib loading. Android needs app-packaged native libraries and an explicit loader path. Automatic sotto building is Linux-only. |
| Serial ports | `rig/src/SerialPorts.cpp` | Non-Windows enumeration searches `/dev/serial/by-id`, `ttyUSB` and `ttyACM`. macOS needs `/dev/cu.*` discovery; Android USB access needs a separate integration. |
| Process discovery | `audio/src/HostProcess.cpp`, `core/src/ear/ProcNet.cpp` | Non-Windows local-host PID discovery uses Linux `/proc`. Do not rely on it for macOS or an Android network client. A remote host's audio is not available merely through its existing TCP connections. |
| Images and denoise | `image/CMakeLists.txt`, `denoise/CMakeLists.txt` | OpenJPEG/WebP/HEIF and RNNoise can be disabled. Cool-chic encoder/decoder remain built with pictures disabled; mobile CPU, memory, thermal and battery costs still need measurement. |
| Tests and releases | `tests/CMakeLists.txt`, `.github/workflows/build.yml`, `packaging/` | Existing fake-host, replay and protocol tests are useful starting points. CI and release recipes currently cover Linux and Windows. Android cross-built executables need a device/emulator runner. |

## Baseline experiments

Host: Apple Silicon (`Darwin arm64`), Apple Clang 17.0.0, CMake 3.31.12.
Android tools found locally: NDK `28.2.13676358`, Clang 19.0.1.
Android baseline target: `arm64-v8a`, API 26, chosen only for this spike;
the supported minimum Android version has not been decided.

Both compilers stop with the default `RAGTIME_WERROR=ON` at
`core/src/globus/Globe.cpp:1236`: `-Wunused-lambda-capture` for `along`.
The experiments below allow warnings to expose further blockers. This
does not change the repository's warning policy or source code.

Android core cross-build: **passed** with `RAGTIME_WERROR=OFF` using the
command below. The default warnings-as-errors build failed as described
above. No Android executable or APK has been run.

The macOS baseline also reports an unused `HALF_TURN` constant at
`tests/GlobusTest.cpp:641`; include that in M1's Clang cleanup.

macOS dependency-minimal build: **passed** with `RAGTIME_WERROR=OFF`.
The linker also warns about duplicate `ragtime_core` library inputs.

Across the full test run and targeted reruns, **37 of 39 tests passed**:

- The initial sandboxed run passed 34/39. Three socket tests aborted with
  `fake host: no KISS port`; all three passed when rerun with loopback
  access (`ragtime_fake_host_test`, `ragtime_session_host_test`,
  `ragtime_replay_test`). These were environment restrictions.
- `ragtime_broadcast_files_test` still fails seven checks, at lines
  1107–1110 and 1700–1703 in `tests/BroadcastFilesTest.cpp`. Seal marks and
  expected repair-request ordering differ. A focused sequential rerun
  reproduced them. Investigate seeded backoff expectations across C++
  standard libraries (`Seals::armAsk` uses `uniform_int_distribution`);
  the root cause is not established.
- `ragtime_sotto_test` still fails its wrong-library error assertion at
  `tests/SottoTest.cpp:274`: it tries Linux's `libm.so.6` on every
  non-Windows OS. macOS cannot load that library, so it never reaches the
  expected missing-symbol error. The codec round trip was skipped because
  libsotto is absent.

These runs used unchanged C++ sources. Logs are in the ignored build
folders: `build-portability/build.log`, `ctest.log`,
`ctest-loopback.log`, `ctest-focused.log`, and
`build-android-arm64-investigation/build.log`.

### Reproduce the macOS dependency-minimal baseline

```sh
cmake -S . -B build-portability \
  -DCMAKE_BUILD_TYPE=Release \
  -DRAGTIME_APP=OFF -DRAGTIME_HAMLIB=OFF \
  -DRAGTIME_PIPEWIRE=OFF -DRAGTIME_PICTURES=OFF \
  -DRAGTIME_HEIF=OFF -DRAGTIME_RNNOISE=OFF \
  -DRAGTIME_BUILD_SOTTO=OFF -DRAGTIME_WERROR=OFF
cmake --build build-portability --parallel 6
ctest --test-dir build-portability --output-on-failure --parallel 4
```

The wxWidgets app is excluded. Hamlib, JPEG 2000/WebP/HEIF, RNNoise and
sotto are excluded; the audio backends are stubs. A green run here cannot
validate those features. `ragtime_sotto_test` can pass without exercising
a codec round trip when libsotto is absent.

The app configure was separately attempted with `RAGTIME_APP=ON` and the
same optional dependencies disabled. It stops at `find_package(wxWidgets)`:
wxWidgets 3.2 or newer is not installed/discoverable on this machine.
The Cocoa UI has not been compiled or exercised.

### Reproduce the Android core cross-build

Set `RAGTIME_NDK` to an installed NDK directory; the local experiment used
`/Users/tvetter/Library/Android/sdk/ndk/28.2.13676358`.

```sh
RAGTIME_NDK=/path/to/android-sdk/ndk/28.2.13676358
cmake -S . -B build-android-arm64-investigation \
  -DCMAKE_TOOLCHAIN_FILE="$RAGTIME_NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 \
  -DCMAKE_BUILD_TYPE=Release \
  -DRAGTIME_APP=OFF -DRAGTIME_TESTS=OFF -DRAGTIME_HAMLIB=OFF \
  -DRAGTIME_PIPEWIRE=OFF -DRAGTIME_PICTURES=OFF \
  -DRAGTIME_HEIF=OFF -DRAGTIME_RNNOISE=OFF \
  -DRAGTIME_BUILD_SOTTO=OFF -DRAGTIME_WERROR=OFF
cmake --build build-android-arm64-investigation \
  --target ragtime_core --parallel 4
```

This builds only a static library, not an APK, JNI bridge, or working
network client. No Android runtime tests have been run. A JNI shared
library experiment should additionally set
`-DCMAKE_POSITION_INDEPENDENT_CODE=ON` for the libraries it incorporates.

## macOS workstream

Use the existing wxWidgets frontend through wxOSX/Cocoa. Upstream lists
Cocoa on macOS as a supported platform; this makes reuse plausible, but
does not prove Ragtime's widgets and event behavior work unchanged.

1. **M1 — Compile and bench the desktop app.** Make Clang builds clean
   with warnings as errors; investigate the broadcast-repair failures and
   remove Linux-only library assumptions from the sotto test. Provision
   wxOSX/Cocoa in an isolated build
   environment; build the app and tests. Exercise Broadcast, Session and
   Files against fake hosts, UTF-8 text, file repair, restart persistence,
   reconnect, dialogs, clipboard, menus and display scaling. Reproduce on
   Apple Silicon first; decide Intel support from demand and build results.
2. **M2 — Platform adapters.** Add native serial-device enumeration and
   bundle-aware sotto loading, and verify the pinned sotto/Codec 2 build.
   Add a macOS `VoiceAudio` backend with capture authorization, device
   selection and hotplug handling. Investigate a shared-input Core Audio
   `EarAudio` backend separately: prefer explicit device selection until
   host-device discovery is proven. Preserve the separation between the
   operator's voice devices and the radio's audio device. Handle denial
   and device changes without affecting modem operation.
3. **M3 — Distributable build.** Create a `.app` with resources,
   `Info.plist`, usage descriptions and correctly located dylibs. Package
   optional codec dependencies and notices; verify loading without the
   developer's package-manager paths. Add macOS CI, then Developer ID
   signing, notarization and a clean-machine install check. Choose the
   minimum macOS version and architecture policy before release.

**First demo exits:** the Cocoa app launches, passes available tests,
exchanges chat and a file with the fake bench, saves history, and reconnects.
Voice, the Ear and direct rig control may remain explicitly unavailable
for this demo; they need separate acceptance checks before parity claims.

## Android workstream

The upstream wxWidgets platform list does not provide an established
Android desktop-app build route. Initial proposal: Kotlin/Compose for the
mobile shell and JNI for the existing C++ engines. This is an architectural
inference, not a toolkit decision already made. Compare it with Qt/QML in
a short decision note if broader UI reuse is a priority; a Qt migration
would also replace the current desktop UI and needs a separate rationale.

1. **A1 — Prove native reuse.** Pin NDK, provisional minSDK and ABIs;
   cross-compile core, then link it into a JNI shared library with PIC.
   Keep JNI ownership, event dispatch, threading and exception boundaries
   explicit. Run existing protocol/golden-vector checks on an emulator and
   a physical ARM64 device. Compile success alone is insufficient.
2. **A2 — Build a vertical slice.** Provide host address, callsign and
   Broadcast/Session/Files navigation suitable for a phone. Implement
   KISS/command/session-stream adapters matching `HostConnection`, with
   framing, buffering, timers and reconnect. Connect to a fake host over
   LAN, then test desktop interop: UTF-8 chat, an explicit session and a
   file in each direction. Use app-private files for core's path-based I/O;
   copy document-provider imports/exports at the platform boundary rather
   than passing `content://` URIs as filesystem paths.
3. **A3 — Lifecycle and persistence.** Exercise rotation, activity
   recreation, process death, screen lock, Wi-Fi loss and network changes.
   Define the foreground-only behavior first; if continuous background
   operation is needed, select a permitted foreground-service type and
   validate current restrictions, notifications and battery costs.
   Explicitly request the permissions needed by the selected APIs and
   target SDK; verify local-network access on supported OS versions.
4. **A4 — Add features from evidence.** Investigate microphone/speaker
   APIs and packaged sotto libraries for voice, then image dependencies
   and Cool-chic costs. Ear monitoring of a remote radio needs an audio
   source/transport design beyond the current client connection. Defer
   local USB serial/Hamlib and an on-device modem to separate spikes.

**First demo exits:** an installable debug APK on an ARM64 device connects
to an external host, exchanges chat and a small file with desktop Ragtime,
retains history, and recovers predictably after disconnect/recreation.
No voice, Ear or local-radio control is required for that demo.

## Initial project checklist

- [x] Clone the GitHub repository into the existing local Ragtime project.
- [x] Audit architecture, platform branches, build switches and release jobs.
- [x] Identify the first warnings-as-errors blocker on both Clang toolchains.
- [x] Record completed macOS baseline build/test results and Android core build results.
- [ ] Resolve the two remaining macOS baseline test failures and Clang warnings.
- [ ] M1: demonstrate the Cocoa app with the fake-host bench.
- [ ] A1: load a JNI library and run core checks on an Android device/emulator.
- [ ] A2: demonstrate phone-to-desktop chat and file transfer over LAN.
- [ ] Decide supported OS/ABI matrix, first-release features and background behavior.
- [ ] Complete adapter spikes and measure CPU/memory/battery before estimating full ports.
- [ ] Write a go/no-go decision and effort ranges from the spikes.

macOS has the shorter architectural route because its GUI toolkit backend
already exists. Android requires more product and adapter work. Effort
estimates remain open until M1 and A1/A2 expose the actual compile, runtime
and UI work; the current evidence does not support calendar commitments.

## Official platform references

- [wxWidgets overview and supported platforms](https://wxwidgets.org/about/)
- [Android NDK with CMake](https://developer.android.com/ndk/guides/cmake)
- [Gradle native build integration](https://developer.android.com/studio/projects/gradle-external-native-builds)
- [NDK C++ runtime choices](https://developer.android.com/ndk/guides/cpp-support)
- [Android document storage](https://developer.android.com/training/data-storage/shared/documents-files)
- [Android foreground-service start restrictions](https://developer.android.com/develop/background-work/services/fgs/restrictions-bg-start)
- [macOS capture authorization](https://developer.apple.com/documentation/bundleresources/requesting-authorization-for-media-capture-on-macos)
- [macOS notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

Platform documentation checked during kickoff. Recheck API and distribution
requirements when selecting target OS versions and implementing each adapter.
