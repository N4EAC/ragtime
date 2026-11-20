Ragtime for macOS
=================

Ragtime is a plain, friendly chat and file-transfer client for Data2G,
the HF data modem by Andrew Rodland, KC2G. https://github.com/baitisj/ragtime

The macOS port is Tim Vetter's work. This build is for Apple Silicon
Macs on macOS 14.2 or later. There is no installer: drag Ragtime.app to
Applications (or anywhere) and open it. To remove Ragtime, delete the app,
the settings file ~/Library/Preferences/Ragtime Preferences and the folder
~/Library/Application Support/Ragtime.

The first time you open it
--------------------------

Ragtime is not signed by an Apple developer, so macOS stops it the first
time with "Apple could not verify Ragtime". Click Done, then open System
Settings > Privacy & Security, scroll down to Ragtime and click Open
Anyway. macOS remembers the choice. (Or, in Terminal:
xattr -dr com.apple.quarantine /Applications/Ragtime.app)

macOS asks for the microphone the first time you record a voice message,
test the microphone or open the Ear. Say Allow, or change it later in
System Settings > Privacy & Security > Microphone.

You need data2g-host
--------------------

Ragtime is only a front end. It talks over TCP to a data2g-host that you
run yourself, on this Mac or another computer on your network; Ragtime
never starts it, never bundles it, and never plays to the radio's sound
card. Its Ear (View > Ear) only records, alongside data2g-host, from the
input data2g-host records from (or one you pick). Get Data2G from
https://github.com/arodland/Data2G and follow its own instructions to run
data2g-host with your radio.

In Ragtime, open Settings > Station and give your callsign and locator, and
the data2g-host address and ports. The defaults (127.0.0.1, KISS port
8100, command port 8300, session data on 8301) match data2g-host's own.

New on the Mac
--------------

Rig control through Hamlib (Settings > Rig) is built in, but hasn't yet
been tried with a radio on a Mac; serial ports are offered as /dev/cu.*.
docs/MACOS.md in Ragtime's source has what has been tried and what not.

The gearshift's figures
-----------------------

The gearshift (Settings > Gearshift) picks a Data2G mode from the modes'
sensitivity thresholds. A data2g-host new enough to send them with its
MODES answer gives them to Ragtime; otherwise pick Data2G's
data2g/codes_data/mode_thresholds.json with Load thresholds... in the
gearshift. Without thresholds Ragtime still chats; it just sends in the
host's own mode.

Voice
-----

The microphone button opens the microphone deck: hold Push to talk (or
the space bar) to record a voice message, and let go to send it. The
radio's sound card is never offered as a microphone or speaker.

Ragtime is MIT licensed: see LICENSE.txt. It includes wxWidgets
(https://wxwidgets.org, the wxWindows Library Licence), Hamlib
(https://hamlib.github.io, LGPL 2.1), OpenJPEG (https://www.openjpeg.org,
BSD), libwebp (https://chromium.googlesource.com/webm/libwebp, BSD),
libde265 and libheif (https://github.com/strukturag, LGPL 3) and RNNoise
(https://github.com/xiph/rnnoise, BSD), each under its licence in the
licences folder. Voice messages are coded with sotto
(https://github.com/baitisj/sotto, MIT), which links Codec 2
(https://github.com/drowe67/codec2, LGPL 2.1); both are dylibs in
Ragtime.app/Contents/Frameworks, with their licences in
Contents/Resources/licences. Ragtime loads them when it starts and works
without them, with no voice coding, so you can swap in your own build.
Ragtime's source and the recipe that builds this zip
(packaging/macos/make-zip.sh) are at https://github.com/baitisj/ragtime,
to build it with your own copies of these libraries.
