# Ragtime for Android — development slice

Native Kotlin/Compose frontend over Ragtime's existing C++17 engines.
Stations, Conversations, Field desk and Files share one connection owner.
The agreed navigation is in [../docs/ANDROID.md](../docs/ANDROID.md).

Build 0.2.0 supports external-host broadcast and session text, addressed
acknowledgements, station discovery, explicit Call/Leave, pause, narrow
modes, reconnect, native chat history, and session and broadcast files up
to 300 KB through Android's document picker. Connect starts a foreground
service, so the connection outlives switching apps. Pictures show inline
and are sent from the + beside the entry line; voice messages arrive as
plain files; rig control, reactions and recent-contact prediction are
not implemented. Field desk shows actual host
mode/channel reports and the gearshift, which picks the mode broadcasts
go in where the modem gives thresholds (Data2G 0.5.1 on); dial frequency
needs a separate rig adapter.
The Hello capability mask says text, status, seals, pings and files, not
reactions, and automatic Hand answers are off until this client supports
the full record set.

To try it with no modem at all, turn on **Demo mode** in Settings:
simulated stations on a net inside the app, behind an amber bar that says
so ([how it works](../docs/ANDROID.md#demo-mode)).

## Build

Pinned development toolchain: JDK 17, Gradle 8.9, AGP 8.7.2, Kotlin 2.0.0,
Compose BOM 2024.10.01, SDK/target 36, NDK 28.2.13676358, CMake 3.22.1.
The provisional minimum is Android 8/API 26; this APK contains ARM64 only.
These pins use the available local toolchain and are not a Play release
or a final OS support policy.

Tests explicitly pin Espresso 3.7.0 because the older Compose test library
otherwise brings in Espresso 3.5.0, whose reflective input lookup fails on
API 37. The fix is documented in the
[AndroidX Test release notes](https://developer.android.com/jetpack/androidx/releases/test#espresso-3.7.0).

Install the SDK components and accept their licences through Android's
SDK manager. Set `JAVA_HOME` to JDK 17 and `ANDROID_HOME` to your SDK
(or create an ignored `local.properties` with `sdk.dir=...`). Then:

```sh
cd android
./gradlew :app:assembleDebug :app:lintDebug
```

The APK is `app/build/outputs/apk/debug/ragtime-android-<version>-b<build>-debug.apk`,
named from `versionName` and `versionCode` in `app/build.gradle.kts`; raise
the build number with every APK handed to someone. Open `android/`
in Android Studio to run it. Configure your separately running
`data2g-host` address, callsign and KISS/command ports; session data uses
command + 1. The app does not bundle, launch, or replace the modem.

The Gradle project builds the existing core with PIC and desktop/audio
dependencies disabled. For pictures it clones OpenJPEG 2.5.3 from GitHub
the first time it configures (`cmake/FetchOpenJpeg.cmake`) and builds it
into `libragtime_jni.so`, so that build needs the network once. JNI uses byte arrays for standard UTF-8, opaque
registered handles, and exception boundaries. All engine calls and three
nonblocking TCP streams belong to one worker. Partial writes remain queued;
KISS/command failure reconnects both with 2/4/8/15/30-second backoff.
The session data stream retries independently every five seconds.

## Runtime policy

Settings (the gear) has the callsign and locator first, then the host and
ports with Connect and Disconnect right under them: Connect checks and
saves those fields and connects to that modem, Disconnect ends the
connection. The host field has a
**localhost** button that fills in 127.0.0.1 and ports 8100 and 8300 for a
Data2G TNC on the same phone. Not connected, the tip on Stations opens the
sheet. Under the callsign it says that the callsign and locator go out
with everything sent, with Privacy beside it.

About has **Privacy** and **Open source licences**. The privacy text is
`docs/ANDROID_PRIVACY.md` and Ragtime's licence is the repository's
`LICENSE`, both copied into the APK when it is built, so the app cannot
drift from them; the Apache License is the asset
`app/src/main/assets/licenses/apache-2.0.txt`. Which notices are owed,
and why, is in [docs/ANDROID_LICENSES.md](../docs/ANDROID_LICENSES.md).

Connect is explicit, and it starts `RadioService`, a foreground service
(special use) with an ongoing notification that opens Ragtime or
disconnects. The connection, the engines and any ragchew belong to that
service's process-wide model, not to the activity: rotating, switching
apps or closing the task's window does not disconnect. Disconnect, the
notification's action, a failed connection or the process dying ends it.
Disconnect and a settings change first leave any ragchew at the modem
(Leave now, sent while the command port is still open), as the desktop
does on quit: closing the sockets alone would leave the modem's ARQ link
up and the next call refused. A process killed outright cannot do that;
a relaunched process requires Connect again and never calls the partner
back by itself. A partial wake lock is held while connected. Android 13
and later ask for the notification permission after the first Connect;
refused, the service still runs. Settings and core chat logs are
app-private; UI drafts and the selected view survive activity recreation
through saved state.

While Ragtime is out of sight, notifications on the channel "Calls and
messages" say that a station called, wrote to the operator with `@`,
spoke in the ragchew or offers a file; a tap lands there, and returning
to the app clears them. "Notify me of calls and messages" in Settings
turns them off and on. The activity is `singleTop`, so a tap never
opens a second Ragtime.

Broadcast numbering is saved before output is handed to a socket. Data is
separated by callsign. A settings change rebuilds the connection; switching
views does not. Broadcast and session drafts remain separate.

## Files

A picked document is copied into the station's own folder
(`files/stations/<call>/outgoing/`), at most 300 KB, and only a file
under that folder is ever sent. Received files go to `received/` beside
it, broadcast parts to `broadcast-parts/`, and the Files rows and kept
parts to `files.log` in the desktop's format. Save a copy writes a
finished file wherever Android's picker says. Session offers wait for
Receive or Decline; a broadcast file waits for Receive, then asks for
the pieces it missed. Forget removes a row, not the file.

## Fake-host tests

Never point the interoperability tests at a real radio. Start the existing
`ragtime_fake_air` bench with both ports free, for example from a dependency-
minimal desktop build:

```sh
./build-portability/tests/ragtime_fake_air --bots 0 --airtime 0
```

Then, with a device or an ARM64 API 35 emulator attached:

```sh
./tools/android-smoke.sh emulator-5554
./tools/android-smoke.sh PHONE_SERIAL 10000
```

The script builds and installs both debug APKs on **only the named device**,
forwards the six loopback fake-host ports, runs the tests, removes those
forwards and puts the app's saved settings back. The second
argument moves the bench's ports on the device (18100, 18300 and so on):
use it on a phone whose own Data2G TNC holds 8100/8300, so no test meets
the real modem. It grants the notification permission, which Connect
would otherwise ask for over the screen the UI tests drive. A new
emulator needs 4 GB (`hw.ramSize`) and a few idle minutes after its first
boot, or Android kills the app for starting too slowly.

Native interop tests exercise UTF-8/emoji broadcasts, acknowledgements,
explicit sessions and delivered text, a session file offered, received and
compared byte for byte, Leave, saved history, and reconnect. Compose tests
exercise the shared conversation draft, separate Broadcast/ragchew drafts,
activity recreation, the active ragchew's Stations → Conversations → Back
flow, and the ragchew kept by the connection service while the activity
is stopped.

The dependency-free `ragtime_android_client_test` also runs under desktop
CTest. It checks fragmented command input, the advertised Hello masks,
UTF-8 JSON escaping, session opening and durable broadcast numbering.
`ragtime_android_files_test` runs two adapters on in-process fake hosts
through session and broadcast file transfers, with no device, and
`ragtime_android_demo_test` runs demo mode's whole net with an adapter on
it. `./gradlew :app:testDebugUnitTest` runs the demo guide's steps on the
JVM, and `AlertTrackerTest` what is worth a notification. `DemoModeTest`,
`AboutTest` (the notices) and `AlertsTest` (a notification while out of
sight) run on the device with the others and need no bench.

Verified on 2026-11-21 with 0.2.0 build 3: both desktop adapter tests
pass with the eight HostLink, Broadcast, Session and file CTest targets
beside them; debug APK assembly and Android lint pass (0 errors, 11
warnings for the provisional toolchain/ABI choices); and the tethered S26 (SM-S948U1, Android 17/API 37) passes the
five fake-host tests, demo mode's, the notices' and the alerts' in 121 s (the broadcast file waits
about 45 s for its seal and repair), over USB on offset ports with its
own TNC left running. The system document picker, the
notification's actions, screen lock, process death and performance are
not yet checked on the phone. Results over the real modems are in
[docs/ANDROID.md](../docs/ANDROID.md#over-real-modems).

## Real-modem tests

Two instrumentation tests run over real Data2G modems. They are skipped
unless `pilotHost` is given, and they need a bench you set up yourself:
two `data2g-host`s whose audio is joined with no sound card
(`--audio-io pipe:IN,OUT` on crossed FIFOs, see
[docs/DATA2G.md](../docs/DATA2G.md)), the phone's Ragtime on one and a
desktop Ragtime with a settings file of its own (`ragtime --config FILE`)
on the other. The waveforms are real; there is no sound device, rig or RF.

`RealModemPilotTest` sends a unique addressed UTF-8 message from the
phone's UI and requires the desktop's acknowledgement over the modems.
Build and install the test APK, then run it on the device you name,
without ADB reverse:

```sh
adb -s DEVICE_SERIAL shell am instrument -w -r \
  -e class org.ragtime.radio.RealModemPilotTest \
  -e pilotHost HOST_ADDRESS -e pilotMarker UNIQUE_TOKEN \
  org.ragtime.radio.test/androidx.test.runner.AndroidJUnitRunner
adb -s DEVICE_SERIAL exec-out run-as org.ragtime.radio \
  cat files/real-modem-pilot.json
```

Its other arguments are `pilotCall` and `pilotLocator` (the phone's,
W1AAA and CN87 by default), `pilotPeer` (the desktop's callsign, W1BBB)
and `pilotKiss` and `pilotCommand` (the host's ports, 18100 and 18300;
session data is the command port + 1). Keep Ragtime open on the phone
throughout: the deadline allows for real transmissions. The report has
the marker, the peer and the time to the acknowledgement.

`RealModemFileTest` takes the same arguments (and `pilotBytes`, 1200 by
default): it calls the peer, sends a session file and waits for Sent,
which the far end gives only after checking the digest, leaves, then
broadcasts a second file. The desktop must take both with no click and
answer promptly: in its settings file, `AutoAccept=PHONE_CALL` and
`AutoReceiveBroadcast=1` under `[Files]`, and `Auto=1` under
`[Gearshift]` (left in its lowest gear a desktop takes 15 s to say
anything, and talks over the phone's file). Compare the desktop's copies
with the SHA-256s in `files/real-modem-file.json`: the phone cannot see
the far end's disk. It takes about four minutes, or longer with
`-e pilotSettle SECONDS`, the time it stays on the air after the
broadcast file has gone, for its seal and any repairs (90 by default; a
file that was talked over needs minutes).

The runs so far are in
[docs/ANDROID.md](../docs/ANDROID.md#over-real-modems). They used helper
scripts that start such a bench on a Mac and, for the phone's own modem,
a test build of Data2G's Android TNC that takes its audio over TCP.
Neither is in this repository: the second is a change to Data2G's own
source.

An APK handed to anyone is the debug build as tested, copied to
`dist/android/` with its SHA-256 and the release notes; `dist/` is not
in the repository.
