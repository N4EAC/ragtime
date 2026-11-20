# macOS port investigation

The macOS implementation now has a development build route and native
adapters. See [MACOS.md](MACOS.md) for current instructions and limits;
the experiments below record the original, unchanged-source baseline.

Started 2026-11-19 for the `finitor/ragtime` checkout, against
`60225d4e7bc91c48d461d705ddcd5ab7c7990b39` (0.3.1-beta).
Working branch: `macos-port`.

## Goal and first recommendation

Establish a build route, a small working client, and a measured list of
remaining work for macOS before committing to full feature parity.
Keep the existing C++ protocol engines and wire formats, using the
wxWidgets Cocoa frontend.

Ragtime connects to an operator's separately running `data2g-host`; it
continues to neither bundle nor launch the modem. Use the existing
fake-host/replay bench for tests, not a real radio. Preserve the operator's
explicit session and rig-control choices described in `CLAUDE.md` and
`docs/PLAN.md`.

This is a kickoff and feasibility record, not a claim that the macOS port
is complete. GitHub Issues is disabled for this repository (the kickoff
issue creation returned HTTP 410); the checklist below is the initial
project tracker.

## Evidence from the checkout

| Area | Evidence | Porting implication |
| --- | --- | --- |
| Shared engines | `core/CMakeLists.txt`, `core/include/ragtime/` | Socket-free C++17 engines for protocol, chat, sessions, files, persistence, spectrum and voice processing are the reuse boundary. Core still includes a Linux `/proc` helper; portable compilation does not make that helper work on other systems. |
| Desktop shell | `app/CMakeLists.txt` | wxWidgets 3.2; GTK dependency is already excluded on Apple. There is no `MACOSX_BUNDLE` target or macOS packaging directory. |
| Transport | `app/src/HostConnection.{h,cpp}`, `RigConnection.{h,cpp}` | Socket and timer adapters use wxWidgets and live outside core. Verify the Cocoa app's three host streams, partial writes and reconnect. |
| Settings | `app/src/Prefs.cpp` | wxConfig and wxStandardPaths are desktop adapters. Verify preferences and application data paths on macOS. |
| Audio | `audio/CMakeLists.txt`, `audio/include/ragtime/audio/` | PipeWire and WASAPI implementations exist. macOS selects `NoEar` and `NoVoice` in this baseline. |
| Voice codec | `audio/src/Sotto.cpp`, `packaging/build-sotto.sh` | Every non-Windows platform currently uses `libsotto.so.0` and `/proc/self/exe`. macOS needs executable/bundle paths and dylib loading. Automatic sotto building is Linux-only. |
| Serial ports | `rig/src/SerialPorts.cpp` | Non-Windows enumeration searches `/dev/serial/by-id`, `ttyUSB` and `ttyACM`. macOS needs `/dev/cu.*` discovery. |
| Process discovery | `audio/src/HostProcess.cpp`, `core/src/ear/ProcNet.cpp` | Non-Windows local-host PID discovery uses Linux `/proc`. Do not rely on it for macOS. A remote host's audio is not available merely through its existing TCP connections. |
| Images and denoise | `image/CMakeLists.txt`, `denoise/CMakeLists.txt` | OpenJPEG/WebP/HEIF and RNNoise can be disabled. Cool-chic encoder/decoder remain built with pictures disabled; measure their resource use on macOS. |
| Tests and releases | `tests/CMakeLists.txt`, `.github/workflows/build.yml`, `packaging/` | Existing fake-host, replay and protocol tests are useful starting points. CI and release recipes currently cover Linux and Windows. |

## Baseline experiments

Host: Apple Silicon (`Darwin arm64`), Apple Clang 17.0.0, CMake 3.31.12.

Apple Clang stops with the default `RAGTIME_WERROR=ON` at
`core/src/globus/Globe.cpp:1236`: `-Wunused-lambda-capture` for `along`.
The experiments below allow warnings to expose further blockers. This
does not change the repository's warning policy or source code.

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
`ctest-loopback.log`, and `ctest-focused.log`.

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
   developer's package-manager paths. Keep GitHub workflows manual-only.
   Add Developer ID signing, notarization and a clean-machine install check. Choose the
   minimum macOS version and architecture policy before release.

**First demo exits:** the Cocoa app launches, passes available tests,
exchanges chat and a file with the fake bench, saves history, and reconnects.
Voice, the Ear and direct rig control may remain explicitly unavailable
for this demo; they need separate acceptance checks before parity claims.

## Initial project checklist

- [x] Clone the GitHub repository into the existing local Ragtime project.
- [x] Audit architecture, platform branches, build switches and release jobs.
- [x] Identify the first warnings-as-errors blocker with Apple Clang.
- [x] Record completed macOS baseline build/test results.
- [x] Resolve the two remaining macOS baseline test failures and Clang warnings.
- [x] M1: demonstrate the Cocoa app with the fake-host bench on Apple Silicon (acceptance results in [MACOS.md](MACOS.md)).
- [ ] Decide supported macOS versions, architectures and first-release features.
- [ ] Complete adapter spikes and measure CPU/memory use before estimating the full port.
- [ ] Write a go/no-go decision and effort ranges from the spikes.

The existing Cocoa toolkit backend provides the macOS architectural route.
The initial investigation leaves effort estimates open until the build,
runtime and UI work has been measured; it does not support calendar
commitments.

## Official platform references

- [wxWidgets overview and supported platforms](https://wxwidgets.org/about/)
- [macOS capture authorization](https://developer.apple.com/documentation/bundleresources/requesting-authorization-for-media-capture-on-macos)
- [macOS notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution)

Platform documentation checked during kickoff. Recheck API and distribution
requirements when selecting target OS versions and implementing each adapter.
