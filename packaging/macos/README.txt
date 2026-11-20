Ragtime for macOS
================

Ragtime is a chat and file-transfer client for Data2G, the HF data modem
by Andrew Rodland, KC2G. https://github.com/baitisj/ragtime

This package is for Apple Silicon Macs running macOS 14.2 or newer.
Unzip it, copy Ragtime.app to Applications, and open it. No package
manager or separately installed libraries are required.

Run Data2G separately, on this Mac or another computer on your network.
Ragtime neither starts nor bundles the modem. Get Data2G and follow its
instructions at https://github.com/arodland/Data2G.

In Settings > Station, enter your callsign, locator, host address and
ports. The defaults match a local data2g-host: 127.0.0.1, KISS 8100,
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
inside Ragtime.app, under Contents/Resources/licences. This package
includes wxWidgets, Hamlib, OpenJPEG, libwebp, libde265, libheif, RNNoise,
Cool-chic, sotto and Codec 2. The sotto and Codec 2 dylibs are in
Contents/Frameworks and can be replaced with compatible builds.

Ragtime's source, dependency pins and the recipe for rebuilding this
package with modified libraries are at https://github.com/baitisj/ragtime,
in packaging/macos/make-zip.sh and docs/MACOS.md.
