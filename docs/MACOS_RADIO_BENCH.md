# macOS USB mock-radio bench

Use this after the native build in [MACOS.md](MACOS.md). The initial
hardware target is two ESP32-S3 USB mock radios, each presenting a sound
card and a CAT serial port. No RF is involved. Keep separate bench
settings and logs for each station.

## Software preflight

Keep the entire Data2G application bundle: its host executable loads Qt
and Hamlib from the bundle's Frameworks directory. Set `DATA2G_HOST` to
the release executable, not the `.app` directory:

```sh
export DATA2G_HOST='/absolute/path/to/Data2G.app/Contents/MacOS/data2g-host'
python3 tools/bench/real_modem_smoke.py --host "$DATA2G_HOST" \
  --output build-macos/real-host-smoke
```

This opt-in POSIX check starts two real hosts, crosses their 8 kHz float
audio streams through named pipes, connects an ARQ session, verifies test
bytes in both directions and disconnects. It opens no sound devices or
rigs. Defaults are KISS ports 18100/18110, command ports 18300/18310 and
data ports 18301/18311. Occupied ports are rejected before starting hosts.
Each run keeps its host logs, command transcripts, executable hash and
result under a new output directory; its child processes and FIFOs are
cleaned up. It is not registered in CTest or GitHub Actions.

The pipe check tests the external modem. Ragtime's application handshake,
session/file engines, voice processing and hardware audio checks are
separate; passing it does not qualify USB timing or the board link.

## Assign one station to each board

Use the firmware project's flashing instructions. The operating mock-radio
connection is the native **USB** jack; the **COM/UART** connection is for
flashing and logs. Start with the IC-705 personality (Hamlib model 3085).
Check the firmware build and USB descriptors in the mock-radio project
before flashing. Descriptor structure alone cannot prove that macOS
enumerates or streams the device.

Connect one board at a time initially. Record its USB serial number,
`/dev/cu.*` CAT port, Data2G input/output device indices, and Ragtime Ear
source name. The mock firmware generates a per-chip USB serial number;
verify that macOS gives the boards distinct identities. Identical display
names are not sufficient to match an audio input to a CAT port.

Use empty, dedicated Data2G config files to list devices:

```sh
"$DATA2G_HOST" --config /absolute/bench/a-data2g.conf --list-audio-devices
```

Choose input and output explicitly. Numeric indices are useful when both
boards have the same name; recheck them after replugging or changing USB
topology. Ragtime uses the stable Core Audio source name printed by
`ragtime_ear_probe`. After both hosts are running, select the matching board
explicitly in each Ear window to resolve host ambiguity.

| Interface | Station A | Station B |
| --- | --- | --- |
| CAT owner | rigctld, board A serial port | rigctld, board B serial port |
| rigctld TCP | 127.0.0.1:4532 | 127.0.0.1:4533 |
| Data2G KISS | 127.0.0.1:8100 | 127.0.0.1:8110 |
| Data2G commands/data | 8300 / 8301 | 8310 / 8311 |
| Modem sound devices | board A input and output | board B input and output |
| Ragtime config | a-ragtime.conf | b-ragtime.conf |

A Data2G GUI already listening on a station's ports is that station's
host. Attach Ragtime to it; do not also launch a headless host there.

## Give CAT and PTT clear owners

One rigctld process owns each board's serial port. Data2G connects to it
with Hamlib model 2 (NET rigctl) and performs CAT PTT. Ragtime uses its
rigctld source to read the dial, with keying and Ear filter writes off.
Direct Hamlib support in the Ragtime bundle is not required.

For station A, replace the serial-port placeholder before running:

```sh
rigctld -m 3085 -r /dev/cu.REPLACE_WITH_BOARD_A -T 127.0.0.1 -t 4532 \
  -P RIG -C serial_handshake=None,dtr_state=OFF,rts_state=OFF
rigctl -m 2 -r 127.0.0.1:4532 f m t
```

Run rigctld in its own terminal; use another for the rigctl readback.
Station B uses its own serial port and TCP port 4533. Verify frequency,
mode and idle PTT before starting the modem. Both boards must have matching
dial frequencies for the link route.

Keep control-line PTT and VOX off for the first CAT tests. CAT remains
available in the mock firmware, while RTS/DTR or VOX would add another
keying source. Configure mock-only settings before rigctld opens the port,
or use the mock's web console afterwards; do not run a serial `mockctl.py`
client against a port rigctld already owns.

For a headless station A, substitute its two audio indices:

```sh
"$DATA2G_HOST" --config /absolute/bench/a-data2g.conf --mycall W1AAA \
  --listen 127.0.0.1 --kiss-port 8100 --command-port 8300 \
  --input-device INPUT_A --output-device OUTPUT_A --sample-rate 48000 \
  --rig-model 2 --rig-device 127.0.0.1:4532 --ptt-method cat --ptt-audio mic
```

Station B uses `W1BBB`, its own config/audio indices, KISS 8110, command
8310 and rigctld 4533. These callsigns are for the no-RF mock bench only.

Create separate Ragtime config files. Station A starts with:

```ini
[Station]
Callsign=W1AAA
[Connection]
Host=127.0.0.1
KissPort=8100
CommandPort=8300
[Broadcast]
Paused=1
AnswerRollCalls=0
[Radio]
Source=rigctld
RigctldHost=127.0.0.1
RigctldPort=4532
[Hamlib]
KeyRig=0
EarFilters=0
```

Station B uses its own callsign, modem ports and rigctld port. Launch with
`Ragtime.app/Contents/MacOS/Ragtime --config /absolute/bench/a-ragtime.conf`.
Keep sending paused while selecting devices. Use operator microphone and
speaker devices for voice, and confirm that both halves of each modem's
sound card are excluded. If an earlier test used the Mac's built-in audio
as the modem, those devices can be remembered as radio devices: after the
modems use the USB boards, use Settings > Voice > Forget to clear obsolete
entries. Currently used radio devices remain excluded.

## Hardware acceptance order

1. One board: confirm audio input/output and CAT enumerate, 48 kHz streaming
   works both ways, and CAT key/unkey returns to idle.
2. Use its monitor/loopback route with noise and fading off; check the
   audio path independently of two-board transport.
3. Run the Ear alongside Data2G. Watch the board's FIFO and underrun
   counters, input rate and PTT state. Confirm closing/reopening Ragtime
   does not stop the modem, and voice cannot select the modem's card.
4. Set both boards to the link route on the same Wi-Fi channel and dial
   frequency. With a clean channel, unpause Ragtime and exchange broadcast
   chat, a session, a checksummed file and a voice clip in both directions.
5. Add noise/fading, then test unplug/replug, board restart and modem
   restart. An explicit missing audio selection must not move to another
   device; PTT must release on normal exit and failure.

Keep the firmware revision/image hash, host executable hash, board/device
mapping, both Ragtime flight logs, modem logs and board telemetry with the
result. USB feedback, clock drift, CAT behaviour and ESP-NOW/UART transport
remain hardware acceptance items.

## Development-Mac evidence, 2026-11-20

Data2G v0.5.3, source commit `ede34e7c15627b09a55c3f6bbae71511cfbb6882`,
was checked as an ARM64 bundle on the development Mac. Its running GUI
served ports 8100/8300/8301. An isolated paused Ragtime profile completed
the command/KISS/data handshake and populated its gearshift from the
host's mode and threshold replies. The Ear followed `data2g-gui` at
48 kHz, and voice discovery excluded the GUI's input/output devices.
The GUI showed zero transmitted bytes and zero audio overflow/underrun
counters during that check. Ragtime exited cleanly; the existing GUI was
left running.

The software-pipe check also passed with two copies of the release host:
32 UTF-8 test bytes and a 21-byte reply arrived exactly, followed by a
normal disconnect. No board, sound device or rig was used in that check.
