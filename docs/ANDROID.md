# Android

Ragtime for Android is a phone app in Kotlin and Compose over the same
C++ engines as the desktop: the protocol, the logs, the station list,
seals and the gearshift are shared, and only the screen is new. It needs
Android 8 or later and an ARM64 phone.

Like the desktop it talks to a separately running Data2G modem and
neither bundles nor starts one. The modem is a `data2g-host` on the
network, or Data2G's own Android TNC, a separate app on the same phone.

Building and testing are in [android/README.md](../android/README.md).

## What it does

- Broadcast on D2G, with `@CALL` and acknowledgements, and a ragchew with
  one station, started with Call and ended with Leave.
- Session files and broadcast files up to 300 KB, with seals and repair.
- Inline pictures, received and sent.
- The gearshift, where the modem gives thresholds.
- Request roll call.
- Notifications of calls, `@` lines, ragchew lines and file offers while
  the app is out of sight.
- Demo mode: simulated stations, to try it with no modem.

Not yet: voice (a clip arrives as a plain file), reactions, editing a
line already sent, refusing a call, the dial and rig control, the Ear,
Globus and Recent work. The app's Hello says what it takes (text,
status, seals, pings and files), so other stations send it nothing it
cannot show.

What each build brought is in
[ANDROID_RELEASE_NOTES.md](ANDROID_RELEASE_NOTES.md), which About shows.

## The four views

- **Stations:** the stations heard on the net, with where they are, how
  strongly they come in and whether they are free. Call station and
  Request roll call are here, and the way back to the current ragchew.
- **Conversations:** Broadcast and the active ragchew, with their history,
  the entry line, pictures and files, and Leave.
- **Field desk:** the same conversation in a denser layout, with the
  mode, the channel's state, the gear and the bandwidth always in sight.
- **Files:** transfers under way, repairs, kept parts and finished files.

The views share one connection, one ragchew, the same history and
transfers, and a draft per conversation. Switching views never connects,
calls, leaves or transmits. Broadcast and the ragchew keep separate
drafts, so text cannot change where it is going by a switch of threads.
Calling another station needs the current ragchew ended first.

The **Ragchewing with CALL** cell in Stations opens that ragchew in
Conversations, and Back returns to Stations without leaving it. A red TX
badge in the top bar follows the modem's PTT reports in every view; its
place is always kept, so the bar never changes height. Settings is
reached from every view.

## Connecting to Data2G

Settings has the callsign and locator first, then the modem's address
and its KISS and command ports, with Connect and Disconnect under them
(session data uses the command port + 1). The **localhost** button fills
in 127.0.0.1, 8100 and 8300, for a Data2G TNC on the same phone. See
[the Data2G interface](DATA2G.md) for the connection itself.

Connect starts a foreground service with an ongoing notification, which
opens Ragtime or disconnects. The connection, the engines and any
ragchew belong to that service, not to the screen: rotating, switching
apps or closing the window does not disconnect. If the modem goes away
Ragtime retries by itself. A partial wake lock is held while connected.
Android 13 and later ask for the notification permission after the first
Connect; refused, the service still runs.

Disconnect first leaves any ragchew at the modem, as the desktop does on
quit: closing the sockets alone would leave the modem's link up and the
next call refused. A process that Android kills cannot do that; started
again, Ragtime waits for Connect and never calls the partner back by
itself.

## How it is built

- **The adapter.** `platform/android/AndroidClient` holds the engines
  (`HostLink`, `BroadcastChat`, `SessionChat`, `Seals`, `BroadcastFiles`,
  `FileTraffic`, `Stations` and the gearshift) and is socket-free, as
  they are. It takes the bytes that came from the modem and gives back
  the bytes to send, hands the screen one JSON snapshot of everything it
  shows, and takes what the operator does as short strings.
- **JNI.** `android/app/src/main/cpp/` builds the core, the picture
  codecs and the adapter into `libragtime_jni.so`.
- **The worker.** In Kotlin one thread owns the engine and the three
  nonblocking TCP streams (`HostTransport`, `RadioModel`). Partial writes
  stay queued. A failed KISS or command stream reconnects both, after 2,
  4, 8, 15 and then 30 seconds; the session data stream retries by
  itself every 5 seconds.
- **Storage.** Everything is in the app's private files, a folder a
  callsign (`files/stations/<call>/`), with the logs in the desktop's
  formats. A broadcast's number is saved before its bytes are handed to
  a socket.

## Files

A picked document is copied into the station's own folder
(`outgoing/`), at most 300 KB, and only a file under that folder is ever
sent. Received files go to `received/` beside it and broadcast parts to
`broadcast-parts/`. Save a copy writes a finished file wherever
Android's picker says.

A session offer waits for Receive or Decline, and a link that drops
keeps the part that came. A broadcast file waits for Receive, is checked
whole by its digest, and asks for the pieces it missed. A file the
operator broadcast says Sent once all of it has gone, and stays open a
while for stations to ask for pieces. Forget removes a row, not the file.

## Pictures

The phone does with pictures what the desktop does. One from the air is
a JPEG 2000 codestream (kind `0x01`) or a Cool-chic stream (`0x02`), and
nothing else is ever decoded. It shows under its line, softer at first
and sharper as more of it comes. Up to 64 KB it comes without asking;
bigger ones wait for Receive like any file. A tap shows it alone, with
pinch to zoom.

To send one, the + beside the entry line offers Picture. Android's photo
picker needs no permission, and Android itself reads whatever it can
(JPEG, PNG, WebP, HEIF, GIF, BMP), the right way up. A sheet shows the
picture as the far end will see it, decoded from the very bytes that
will go, with its longest side (320, 640 or 1024) and its size on the
air (4, 8, 16 or 32 KB) to choose, and for a broadcast about how long it
will take in the gear in effect. It is shrunk with the desktop's
downscaler and sent as progressive JPEG 2000.

The codecs are the desktop's own (`image/`): OpenJPEG, built into the
app's library (`cmake/FetchOpenJpeg.cmake`), and the Cool-chic port.
`platform/android/AndroidPictures` is the bridge, pixels in and pixels
out. A picture is decoded on a thread of its own, since a Cool-chic one
takes seconds.

Not there: sending Cool-chic (fitting one takes minutes of CPU), and the
stop sign for a picture that is good enough. A picture that is one flat
colour is not sent: it codes to so few bytes that the shared codec takes
it for a picture not yet arrived.

## Gearshift

The desktop's gearshift (`core/src/gearbox/`) picks the mode the phone's
broadcasts go in: lines, check-ins, files and pictures on D2G, and the
gears a seal's repairs may use. It is fed as the desktop feeds it
(`platform/android/AndroidGear.cpp`): the modem's `MODES` with their
thresholds, the weakest station heard in 15 minutes, and the 500 Hz
switch. A ragchew's speed is the modem's own affair.

The gears exist only where the modem gives thresholds, Data2G 0.5.1 or
later. With an older modem there are none, the button says so, and
Ragtime sends in the mode the modem is set to. There is no thresholds
file to load on the phone.

Auto is on until the operator taps a gear; the choice, Auto and the path
are kept. Until a station has been heard Auto has nothing to go by, and
the phone sends in the mode the modem is set to and not in the ladder's
lowest gear, which on a real modem can be 29 bit/s: half a minute of
transmitting to say one line.

Field desk shows the gear in effect, its speed and the weakest path. A
tap opens the gearshift: Auto, the path (Calm, Good, Poor, Disturbed)
and the ladder, each gear with its speed, the S/N it needs, its time per
KB and how many dB short the weakest path is for it.

Left out: the width chips (the 500 Hz switch is the only width control),
the noise-floor line (it needs a rig's S-meter), the map of modes, and a
repair gear per file or per station.

## Notifications

While no Ragtime screen is in sight, a notification on the channel
"Calls and messages" tells the operator that a station has called (the
modem answers a ragchew call, so this is the first they hear of it), has
written to them with `@`, has said something in their ragchew, or offers
a file. A tap lands on that ragchew, on Broadcast or on Files, and
coming back to Ragtime takes the notifications down.

`AlertTracker` (`Alerts.kt`) finds them by comparing each snapshot with
what it has already seen. What was there when the connection started,
and what a new ragchew reads back from its log, alerts nobody. There is
one notification a station and kind, its newest line replacing the
last.

Settings has the switch, "Notify me of calls and messages", on by
default. When Android itself is blocking Ragtime's notifications the
switch says so and offers Android's settings.

## Demo mode

Demo mode is for trying Ragtime with no modem, and for a store reviewer
who has none. It runs the real engines against a net inside the app, so
nothing on the screen is faked: `platform/android/DemoAir` puts the
tests' fake data2g-host (`tests/FakeHost.*`) on free loopback ports for
the operator, with four made-up stations beside it, each a real
`HostLink`, `BroadcastChat`, `Seals`, `BroadcastFiles` and `SessionChat`.
The app connects to it exactly as it does to a modem. Nothing of
Data2G's is in it, no audio is made and nothing reaches a modem or a
radio.

- The stations check in one after another within ten seconds, each with
  a line. They never call the roll themselves (a roll call holds the
  channel for a window of Hands), and they do answer the operator's.
- A line to everyone is answered by one station, in turn; a line to
  `@CALL` is acknowledged and answered by that station.
- Three of them take a call, greet, answer what is typed, offer their
  bulletin when a file is mentioned, save a file sent to them and say
  so, and leave after 73. One broadcasts a bulletin file 40 s in and
  every 6 minutes, for Files to take.
- A burst keys the pretend transmitter for 1.5 s, so the TX badge shows,
  and session bytes cross at 400 a second, so a file shows its progress.

The stations are W1QSO, K7QTH, DL3QRM and F5QRN, with made-up operators'
names. Nobody should hold those callsigns: the prefixes are real and the
suffixes are Q signals. The FCC assigns no suffix from QRA to QUZ, so
the two American ones cannot exist. No such rule was found for the
German and the French one; look them up in those countries' registers
before the app is published.

On the screen an amber bar ("DEMO MODE  Simulated stations; nothing is
sent.") lies across the very top and opens Settings. The Demo mode
switch, last in Settings, is the demo: turned on it starts at once,
turned off it ends at once, and Connect and Disconnect, which are for
the operator's own modem, are not shown while it is on. It cannot be
turned on from a ragchew on the air.

A guide's bubble suggests what to try next (`DemoGuide.kt`). It works
out the step from the snapshot alone, so it survives rotation and an
operator who wanders off. A tap on a bubble with a suggested line puts
that line in the right entry field for the operator to send. Hide tips
puts the bubbles away until the next demo, and they step aside while the
keyboard is up.

The demo keeps to itself. It uses the callsign in Settings, or DEMO when
there is none; a demo station with the operator's own callsign is left
out. Its logs and files live in `files/demo/`, wiped at each start,
never in the operator's own folder. The saved address and ports are not
touched, and when it ends the engine is closed and the screen cleared,
so nothing a simulated station said is left to be taken for the air.

## Licences and privacy

About has Privacy and Open source licences, the notices the licences and
Google Play ask to be in the app. Under the callsign, Settings says that
the callsign and locator go out with everything sent. The texts are
[ANDROID_PRIVACY.md](ANDROID_PRIVACY.md) and the repository's `LICENSE`,
copied into the APK when it is built; which notices are owed, and what
Google Play asks for, is in [ANDROID_LICENSES.md](ANDROID_LICENSES.md).

The launcher icon is the desktop's leaf going digital, as an adaptive
icon with a themed layer; `tools/icon/make_android_icon.py` makes its
layers, and the notifications' small leaf, from `app/res/ragtime.svg`.
