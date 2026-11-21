# Android port

Started 2026-11-20. The shared C++ engines are the reuse boundary; the
phone UI is a separate frontend.

## Agreed UI direction

The phone has four primary destinations:

- **Stations:** stations heard on the net, recent contacts, and entry
  points for calling a station or returning to the current ragchew. Roll
  call, which asks every station to check in, is asked for here (it is an
  action, not a setting: Tim, 2026-11-21).
- **Conversations:** Broadcast and the active one-to-one ragchew, with
  message history, a composer, attachments, and session Leave controls.
- **Field desk:** the same selected conversation with the reported dial,
  channel status, gear, and bandwidth always visible in a denser layout.
- **Files:** active transfers, repairs, kept parts, and completed files.

Stations, Conversations, and Field desk are complementary views in one
app. They share one host connection, one active ragchew, the same message
history and transfer state, and a draft per conversation. Switching views
does not connect, call, leave, or transmit.

The entire **Ragchewing with CALL** cell in Stations opens that session
in Conversations. Back returns to Stations without leaving the session.
Conversations and Field desk preserve the selected Broadcast/session
thread and its draft when switching between them. Broadcast and session
drafts remain separate so text cannot change its destination by switching
threads. Calling another station requires ending the current ragchew.

**Demo mode** (Tim, 2026-11-20) is a switch in Settings for trying
Ragtime with no modem: a coloured bar across the very top says so, the
stations are simulated, and a guide's bubbles suggest what to type next.

Radio and host controls are accessible from every destination. Field desk
keeps operational details visible; the other views keep more space for
stations and messages. File cards in a conversation link to Files and
the same transfer's details.

## Current implementation status

The Kotlin/Compose app and JNI bridge are in `android/` and
`platform/android/`; this is Android build 0.2.0 (build 3). The ARM64
debug APK builds against and targets SDK 36 (Android 16, what Google
Play asks of a new app), with API 26 as the provisional minimum. What a
version brought is in [ANDROID_RELEASE_NOTES.md](ANDROID_RELEASE_NOTES.md),
which About shows.
It uses the existing Broadcast and Session engines, logs, station model
and seals. Build and test instructions and runtime limits are in
[android/README.md](../android/README.md).

All four destinations work. Files (2026-11-20) sends and receives session
files (offer, Receive, Decline, Cancel, progress, kept parts, Send rest)
and broadcast files (digest check, repairs asked for), through Android's
document picker in and out, up to 300 KB, with the history kept in
app-private storage. A finished session file has its line in the ragchew.
The Hello now says the phone takes session and broadcast files; it still
leaves out reactions. Pictures show inline (below); voice messages
arrive as plain files.

**Pictures** (2026-11-21). What the desktop does with pictures, the phone
does: one from the air is a JPEG 2000 codestream (kind `0x01`) or a
Cool-chic stream (`0x02`), nothing else is ever decoded, and it shows
under its line in Broadcast or the ragchew, softer at first and sharper
as more of it comes. Up to 64 KB it comes without asking, from anyone;
bigger ones wait for Receive like any file. A tap shows it alone on
black, with pinch to zoom. To send one, the + beside the entry line
offers Picture: Android's photo picker (no permission), and Android
itself reads whatever it can (JPEG, PNG, WebP, HEIF, GIF, BMP), turned
the right way up. The sheet shows the picture as the far end will see
it, decoded from the very bytes that will go, with its longest side
(320, 640 or 1024; 640 as on the desktop) and its size on the air (4, 8,
16 or 32 KB) to choose. It is shrunk with the desktop's
detail-preserving downscaler and coded as progressive JPEG 2000, named
`<picked name>.j2k`.

The codecs are the desktop's own (`image/`): OpenJPEG 2.5.3, fetched and
built into the app's library (`cmake/FetchOpenJpeg.cmake`), and the
Cool-chic port. `platform/android/AndroidPictures` is the bridge, pixels
in and pixels out, and holds no client: a picture's bytes are fetched on
the link's worker (`pictureBytes`: from the file engines while it moves,
from the station's own folder after) and decoded on a thread of their
own, since a Cool-chic picture takes seconds. Not there yet: sending
Cool-chic (its fitting takes minutes of CPU) and the stop sign for a
picture that is good enough. A picture with nothing in it (one flat
colour) is not sent: the shared codec takes so small a codestream for a
picture not yet arrived.

Connect starts a foreground service with an ongoing notification (open,
Disconnect), so the connection and a ragchew outlive switching apps. A
red TX badge in the top bar follows data2g-host's PTT reports in every
view. Settings shows the version and build, with About and release
notes; About has Privacy and Open source licences, the notices the
licences and Google Play ask to be in the app
([the audit](ANDROID_LICENSES.md), [the privacy text](ANDROID_PRIVACY.md)). Field desk shows the real mode, bandwidth and channel reports, and the gear.
Dial and rig integration, voice, reactions, automatic Hand answers and
recent-contact prediction are not implemented.

**Gearshift** (2026-11-21). The desktop's gearshift (core `gearbox`)
picks the mode the phone's broadcasts go in: lines, check-ins, status,
files and pictures on D2G, and the gears the seals' repairs may use. It
is fed as the desktop feeds it (`platform/android/AndroidGear.cpp`):
data2g-host's MODES with their thresholds, the weakest station heard in
15 minutes, and the 500 Hz switch. The gears exist only where the modem
gives thresholds, Data2G 0.5.1 on; with an older one there are none, the
button says so, and Ragtime sends in the mode the modem is set to, as it
did before. There is no thresholds file to load on the phone. Auto is on
until the operator taps a gear, and the choice, Auto and the path are
kept. Until a station has been heard Auto has nothing to go by, and the
phone sends in the mode the modem is set to rather than the ladder's
lowest gear: on the real modems that gear is 29 bit/s, half a minute of
transmitting to say one line (found on the bench, 2026-11-21). Field desk shows the gear in effect, its speed and the weakest
path, and a tap opens the gearshift: Auto, the path (Calm, Good, Poor,
Disturbed), and the ladder, each gear with its speed, the S/N it needs,
its time per KB and how many dB short the weakest path is for it. A
ragchew's speed is the modem's own affair. The picture sheet now says
about how long a picture for D2G will take in the gear in effect. Left
out: the width chips (the 500 Hz switch is the only width control), the
noise-floor line (it needs a rig's S-meter), the map of modes, and a
per-file or per-station repair gear. Checked by
`ragtime_android_gear_test` on the desktop and `GearTest` on the S26.

**Alerts** (2026-11-21). While no Ragtime window is in sight, a
notification on its own channel, "Calls and messages", tells the
operator that a station has called (a ragchew is answered by the modem,
so this is the first they hear of it), has written to them with `@` on
D2G, has said something in their ragchew, or offers them a file. A tap
lands on that ragchew, on Broadcast or on Files, and coming back to
Ragtime takes the alerts down. `AlertTracker` (`Alerts.kt`) finds them
by comparing each snapshot with what it has already seen: what was there
when the connection started, and what a new ragchew reads back from its
log (the snapshot marks those lines `past`), alerts nobody. One
notification a station and kind, its newest line replacing the last. In
demo mode the title says "Demo:". Settings has the switch, "Notify me of
calls and messages", on by default and remembered; when Android itself
is blocking Ragtime's notifications the switch says so and offers
Android's settings. There is no way yet to refuse calls. Checked by `AlertTrackerTest` (JVM) and `AlertsTest` on the S26,
on demo mode's net.

What has been checked, and where:

- Desktop CTest in a tree with the codecs (`-DRAGTIME_FETCH_OPENJPEG=ON`):
  `ragtime_android_pictures_test`, a picture coded within its bytes and
  shown from any first part, sent in a ragchew and on D2G, taken without
  asking and shown from its key and from its path, and nothing read from
  outside the station's folder. On the S26: `PicturesTest` (the codecs
  through JNI; a picture made ready, its size changed, broadcast, shown
  inline and opened) and the interop test's picture between two engines.
- Desktop CTest, no device: `ragtime_android_client_test` (fragmented
  command input, the Hello masks, UTF-8 JSON, durable numbering) and
  `ragtime_android_files_test` (two adapters on fake hosts: a session
  file offered, received and checked byte for byte, one declined, a
  broadcast file received and checked, rows kept across a restart and
  read back once, paths outside the station's folder refused).
- The tethered S26 (SM-S948U1, Android 17/API 37), 0.2.0 build 3, against
  the loopback fake-host bench over USB (2026-11-21, 121 s with demo
  mode's, the notices', the alerts', the roll call's, the pictures' and
  the gearshift's tests (170 s in all), 12 passed, the two real-modem
  tests skipped; the bench's tests run twice on one bench with no restart):
  the packaged-JNI interop test (UTF-8 and emoji broadcast, addressed
  acknowledgements, session text, a session file saved byte for byte
  with its line in the ragchew, Leave, a broadcast file taken with
  Receive and whole by its digest, a path outside the station's folder
  refused, saved history, reconnect); the document test (import into the
  station's folder with its name kept, the 300 KB limit, empty files,
  export, nothing outside the app's files exported); and the Compose
  tests (all four destinations, shared and separate drafts, activity
  recreation, a picked document broadcast and shown in Files and
  Broadcast, Stations → ragchew → Back, the ragchew kept by the
  connection service while the activity is stopped, and Disconnect
  ending the ragchew at the far end). The phone's own Data2G TNC stayed
  up on 8100/8300 throughout: the bench was forwarded on ports 10000
  higher (`tools/android-smoke.sh SERIAL 10000`), and the phone's saved
  settings were put back after.
- The same phone over real modems, see
  [Over real modems](#over-real-modems): the addressed Unicode message
  and its ACK, a session file and a broadcast file.
- The chat-only build earlier passed its tests on an ARM64/API 35
  emulator. 0.1.0 has not run on an emulator: a new google_apis AVD at
  the default 1.5 GB killed the app for slow startup while its first
  boot settled (no app exception), and the phone was used instead. Give
  an AVD 4 GB and let it go idle first.

Android lint: 0 errors, 11 warnings (the provisional SDK and dependency
pins, ARM64-only packaging). The launcher icon is the desktop's leaf
going digital on the cover's cream, as an adaptive icon with a themed
(monochrome) layer; `tools/icon/make_android_icon.py` makes its layers
and the notifications' small leaf from `app/res/ragtime.svg`. A launcher
cuts icons to its own shape, so the desktop's rounded square and red rim
are not in it. The UI test
dependency pins Espresso 3.7.0 for API 37.

Still to do: the system document picker and Save a copy by hand on the
phone (the tests call what the picker's result calls, not the picker),
the notification's own Disconnect action, screen-off and process-death
behaviour of the service, kept parts and Send rest after a dropped link,
a file from the desktop to the phone over the real modems, and larger
files.

### Demo mode

Demo mode runs the real engines against a net inside the app, so nothing
on the screen is faked: `platform/android/DemoAir` puts the bench's fake
data2g-host (`tests/FakeHost.*`, loopback sockets) on free ports for the
operator and four made-up stations beside it (W1QSO, K7QTH, DL3QRM and
F5QRN), each a real HostLink, BroadcastChat, Seals,
BroadcastFiles and SessionChat on a thread of the demo's own. The app
connects to it exactly as it does to data2g-host. Nothing of Data2G's is
in it, no audio is made and nothing reaches a modem or a radio.

- The stations check in one after another within ten seconds, each with
  a spoken line its Hello rides on. They never call the roll themselves:
  a roll call holds the channel for a window of Hands, and the net would
  take half a minute to come up. They do answer the operator's roll call.
- A line to everyone is answered by one station, in turn; a line to
  `@CALL` is acknowledged and answered by that station.
- Three of them take a call, greet, answer what is typed, offer their
  bulletin when a file is mentioned, save a file sent to them and say so,
  and leave after 73. W1QSO broadcasts a bulletin file 40 s in and every
  6 minutes, for Files to take with Receive.
- Nobody holds the stations' callsigns (Tim, 2026-11-21). The prefixes
  are real, two American and two European, and the suffixes are Q
  signals: the FCC assigns no suffix from QRA to QUZ, so the American
  two cannot exist. For DL3QRM and F5QRN no such rule was found; look
  them up in the German and French registers before the app is
  published. The operators' names are made up as well.
- A burst keys the pretend transmitter for 1.5 s, so the TX badge shows,
  and session bytes cross at 400 a second, so a file shows its progress.

On the screen: the amber bar ("DEMO MODE  Simulated stations; nothing is
sent.") covers the status bar's strip too and opens Settings. The Demo mode
switch, last in Settings above its closing note, is the demo (Tim,
2026-11-21): turned on it starts the
demo and the sheet makes way, turned off it ends it at once, and Connect
and Disconnect, which are for the operator's own modem, are not shown
while it is on. It cannot be turned on from a ragchew on the air. The
guide's bubble (`DemoGuide.kt`, `demoTip`) works out the next step from
the snapshot alone, so it survives rotation and an operator who wanders
off, and a tap on a bubble with a suggested line puts that line in the
right entry field (Broadcast or the ragchew) for the operator to send.
Hide tips puts the bubbles away until the next demo, and they step aside
by themselves while the keyboard is up, when there is room for the
conversation and the entry field and little else.

The demo keeps to itself. It uses the callsign in Settings, or DEMO
when there is none; its logs and files live in `files/demo/`, wiped at
each start, never in the operator's `files/stations/`; the saved host and
ports are not touched; and when it ends the engine is closed and the
screen cleared, so nothing a simulated station said is left to be taken
for the air. A demo station with the operator's own callsign is left out.
While the demo runs the TX badge lights as it does on a modem; its place
in the bar is always kept, so the bar never changes height.

Checked: `ragtime_android_demo_test` on the desktop (the whole net with
an adapter on it: check-ins and locators, both replies, a call, a
ragchew, a file each way, 73, the broadcast bulletin whole, the
callsign clash, the runner's thread); `DemoGuideTest` (JVM, the guide's
steps in order); and `DemoModeTest` on the S26, through the real sheet
(the switch, the bar, the stations, bubbles filling the entry field,
both replies, a call and a ragchew, the saved host untouched, the screen
cleared after). Not yet: the file steps of the tour on the device by
hand, and a demo left running for hours.

## Initial milestones

1. Link the shared core into a JNI library and exercise protocol checks
   on an Android emulator and an ARM64 device. Choose and document the
   initial SDK, NDK, and ABI matrix with the build.
2. Build the connected phone shell and a foreground-only network client
   for an external Data2G host. Reuse the existing Broadcast and Session
   engines. Verify chat against fake hosts and desktop Ragtime.
3. Add file transfer through app-private paths and Android document
   import/export, then verify repairs, persistence, reconnect, activity
   recreation, and network loss. Keep media codecs, audio, and local USB
   radio access as separately validated follow-up work.

## Over real modems

Fake hosts prove the protocol; whether a line gets through is for real
modems to say. The bench for that has no radio: two Data2G modems whose
audio is carried between them as samples (crossed FIFOs with
`--audio-io pipe:IN,OUT`, or TCP between two machines), with a Ragtime
on each. Callsigns and messages are made up; delivery and
acknowledgements come from real modem decoding. The opt-in tests and
what they need are in
[android/README.md](../android/README.md#real-modem-tests).

Two set-ups were used, both with Data2G 0.5.3 (`ede34e7`) and an S26:

1. **Both modems on the Mac.** The phone's Ragtime reaches one host over
   Wi-Fi and a desktop Ragtime has the other. This proves the app
   against a real host across the LAN, not a phone's modem.
2. **The phone's own modem.** Data2G's Android TNC runs behind Ragtime
   on the phone (localhost, 8100/8300/8301), and its audio goes over the
   LAN to the Mac's modem and desktop Ragtime. For this the TNC was a
   test build that takes its audio from TCP instead of the microphone
   (8 kHz mono float32, with no jitter or clock-drift handling). That
   build and the scripts that start the Mac's side are not in this
   repository.

### Results

- **2026-11-20, set-up 1, the chat-only build.** An addressed broadcast
  with `café 🚀` reached Acknowledged about 9 s after Send, and the
  desktop's log has the exact text.
- **2026-11-20, set-up 2.** The same check with Data2G running behind
  Ragtime on the phone: acknowledged in 9.4 s.
- **0.1.0 build 2, 2026-11-20, set-up 2.** `RealModemPilotTest`:
  acknowledged 7.4 s after Send. `RealModemFileTest`: connected 4.5 s
  after the call; a 1,200-byte session file Sent and checked 32 s later;
  after Leave, a 1,200-byte broadcast file's pieces had gone in 89 s,
  the Mac asked for a repair ("Repairing, 1 station asked" on the phone)
  and saved it whole. Both files on the Mac have the SHA-256 the phone
  recorded.
- **0.2.0 build 3, 2026-11-21, set-up 2**, with the gearshift picking
  the mode from the thresholds the real modem gives. The pilot:
  acknowledged 7.5 s after Send. The file test, staying on the air
  5 minutes (`-e pilotSettle 300`): connected in 4.3 s, the session file
  Sent and checked at 34 s, and the broadcast file saved on the Mac
  within a minute of the ragchew ending. Both copies match.

### What the runs taught

- **Auto with nobody heard** sat in the ladder's lowest gear, which on
  the real modems is fsk16r25 at 29 bit/s: 34 s of transmitting for one
  line, and an acknowledgement after 39 s instead of 7. The phone now
  sends in the modem's own mode until a station has been heard.
- **A desktop left in its lowest gear** takes 15 to 18 s to say
  anything. When the ragchew ended both stations announced it at once,
  and the Mac's announcement covered the phone's whole broadcast file.
  The repair exchange that follows such a loss needs minutes at that
  speed, and the test left the air after 90 s, reporting a pass with no
  file on the Mac. The bench's desktop is now on Auto, the test's time
  on the air is an argument, and a pass still needs the Mac's copy
  compared by hand: the phone cannot see the far end's disk.
- **A seal from before a restart** cannot be repaired: the Mac, hearing
  seal 3, asked for the seal 2 it had never heard, and the phone refused
  it as no longer held. That is the engines' rule, the same on the
  desktop, and cost nothing here.
- **The first file run** sent random bytes named `.bin`, which nothing
  can open; the test now sends readable `.txt`.

One thing not explained: a desktop Ragtime that had been up three
hours, through a Hand, a toggle of its 500 Hz switch and a call that was
refused (NOT_LISTENING), heard an addressed line and never transmitted
its acknowledgement. A fresh instance acknowledged the same line in
20 ms. It is the desktop's behaviour, not the phone's; the flight
recording of that run is kept.

Still not on the air: pictures, a file from the desktop to the phone,
files over a few kilobytes, an impaired channel, and the phone with its
screen off.
