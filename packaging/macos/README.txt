Ragtime for macOS
================

Ragtime is a chat and file-transfer client for Data2G, the HF data modem
by Andrew Rodland, KC2G. https://github.com/N4EAC/ragtime

This package is for Macs running macOS 27 or newer: the zip named
arm64 is for Apple Silicon, the one named x86_64 for Intel. Unzip it,
copy Ragtime macOS.app to Applications, and open it. No package manager or
separately installed libraries are required.

If Ragtime is signed ad hoc rather than with a Developer ID, macOS stops
it the first time with "Apple could not verify Ragtime". Click Done, then
open System Settings > Privacy & Security, scroll down to Ragtime and
click Open Anyway. macOS remembers the choice.

Run Data2G separately, on this Mac or another computer on your network.
Ragtime neither starts nor bundles the modem. Get Data2G and follow its
instructions at https://github.com/arodland/Data2G.

In Settings > Station, enter your callsign and locator. In Settings >
Soft TNC, enter the external modem address and ports. The defaults match a local data2g-host: 127.0.0.1, KISS 8100,
commands 8300 and session data 8301.

Voice and the Ear
-----------------

Settings > Voice selects your microphone and speakers. Devices used by a
local Data2G modem and remembered radio devices are excluded from voice.
View > Ear listens to a local input without playing audio. With more than
one possible modem input, select the radio in Listen to.

macOS requests microphone access when you first record voice or open the
Ear. If denied, enable Ragtime in System Settings > Privacy & Security >
Microphone and retry. A remote modem connection does not carry its audio.

Libraries and source
--------------------

Ragtime is MIT licensed (LICENSE.txt). Third-party licence notices are
inside Ragtime macOS.app, under Contents/Resources/licences. This package
includes wxWidgets, Hamlib, OpenJPEG, libwebp, libde265, libheif, RNNoise,
Cool-chic, sotto and Codec 2. The sotto and Codec 2 dylibs are in
Contents/Frameworks and can be replaced with compatible builds.

Ragtime's source, dependency pins and the recipe for rebuilding this
package with modified libraries are at https://github.com/N4EAC/ragtime,
in packaging/macos/make-zip.sh and docs/MACOS.md.
