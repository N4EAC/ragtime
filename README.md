# Ragtime macOS

A plain, friendly chat and file-transfer client for
[Data2G](https://github.com/arodland/Data2G), the HF data modem by Andrew
Rodland, KC2G. Ragtime is a sibling of
[Glissando](https://github.com/baitisj/glissando), Jeff Baitis' (AG7EW)
chirp-mode chat app, and is named for the same musical family and for the
ragchew.

Ragtime helps you keep in touch. Every burst it hears teaches it when,
and on which band, each of your friends gets through, so it can tell you
who is likely around now, and the best time of day to check in to the
D2G net to find them.

This is the macOS edition based on [upstream Ragtime](https://github.com/baitisj/ragtime),
with our native macOS improvements and an **external Soft TNC**. It does not
contain, start, or bundle a Data2G modem. The separately maintained
[bundled edition](https://github.com/N4EAC/ragtime-data2g) remains available.

## Set up the Soft TNC

1. Start Data2G separately, locally or on another computer.
2. Open **Settings > Station** and enter your callsign and locator.
3. Open **Settings > Soft TNC** and set the modem address and ports:

   | Setting | Data2G default |
   |---|---|
   | Address | `127.0.0.1` |
   | KISS port | `8100` |
   | Command port | `8300` |
   | Session data port | command port + 1 (`8301`) |

4. Configure radio audio and PTT in the external modem. Configure optional
   frequency readback/tuning under **Settings > Rig** in Ragtime.

Keep the KISS, command and session data ports distinct. Only one client
should control the same external modem. Running the bundled and external
editions together is supported with separate modem ports and audio devices.

## Preserved macOS features

- Native Core Audio microphone/speaker selection and Ear input.
- Voice messages through the separately licensed sotto codec libraries.
- Hamlib and rigctld integration, including bundled rigctl/rigctld tools.
- Editable CAT frequency field: `7100` means 7.100 MHz; Enter tunes,
  Escape restores readback, and CAT polling preserves an unsent edit.
- Editable preset file with per-frequency comments and stable popup width.
- Persistent Clear controls, Recent Work, and session logs with a folder menu.
- Modeless picture preview, About component revisions, and plain MIT credit text.
- Blue Earth icon and short startup animation, with Reduce Motion support.

There are no built-in modem audio/TCI controls or built-in constellation/
decode monitor. The external modem owns those facilities.

## Separate installation and settings

The app is **Ragtime macOS.app**, with bundle identifier
`io.github.n4eac.ragtime.macos`. Settings, frequency lists, message history
and logs are separate from the bundled app: data is kept under
`~/Library/Application Support/RagtimeMacOS/`. Settings are not copied
from or written back to the bundled edition. `--config PATH` remains
available for a separate test configuration.

## Modem compatibility

**External Data2G** is the supported protocol. Ragtime uses Data2G-specific
broadcast-group, mode and acknowledgment commands in addition to its
KISS and VARA-style command/data sockets.

Mercury and MODEM73 are candidates for follow-up adapters. Mercury has
VARA-style command/data sockets and a KISS broadcast port, but lacks
Data2G's group/ACKMODE extensions. MODEM73 has standard KISS on port 8001
and a length-prefixed JSON control API on 8073. Selecting their ports
alone does not make the current Data2G transport compatible. These modems
are not advertised as working profiles in this build. Both radio ends
must use a compatible modem and application protocol.

Sources: [Mercury TNC interface](https://github.com/Rhizomatica/mercury/blob/mercuryv2/docs/TNC.md),
[MODEM73 quickstart](https://modem73.app/quickstart.html).

## Build on macOS

Apple Silicon, macOS 27 or later. Install dependencies:

```sh
brew install cmake ninja pkg-config wxwidgets hamlib openjpeg webp libheif
bash packaging/macos/build-macos.sh
```

The build recipe builds the pinned sotto libraries first, then configures
Ragtime. No Data2G checkout or Qt installation is needed. Build output is
`build-macos/app/Ragtime macOS.app`. To prepare an app for another computer:

```sh
ditto 'build-macos/app/Ragtime macOS.app' 'dist/Ragtime macOS.app'
bash packaging/macos/deploy.sh 'dist/Ragtime macOS.app'
```

The app is ad hoc signed; it is not notarized by Apple.

```sh
ctest --test-dir build-macos --output-on-failure
```

Linux and Windows source support follows upstream; this edition's packaged
build and independent settings are for macOS.

## Licence and credits

Ragtime is MIT licensed; see [LICENSE](LICENSE). Original Ragtime by
Jeff Baitis (AG7EW), upstream macOS port by Tim Vetter, macOS edition
maintenance by Eduardo (N4EAC). The bundled **voice codec**, not a modem,
is sotto v3.8 at `e707134`, with v3.10 encoder improvements. Its Codec 2
dependency is LGPL-2.1; the app carries the codec libraries and licences
and loads them at runtime.

See [CHANGELOG.md](CHANGELOG.md) for this edition's changes.
