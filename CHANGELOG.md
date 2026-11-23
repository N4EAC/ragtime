# Changelog

## 0.4.0 macOS edition — 2026-11-23

- Create an independent macOS edition from upstream Ragtime through
  `00ac994`, retaining the general improvements from our macOS work.
- Restore external Data2G Soft TNC setup in a dedicated Settings page.
- Remove the built-in modem, TCI/audio controls and constellation monitor.
- Use an independent Ragtime macOS app identity, settings, logs and install path.
- Retain Core Audio voice/Ear support, CAT entry and presets, clear controls,
  session logs, rigctl, pictures, About revisions, and the startup animation.
- Add the blue Ragtime/macOS Earth icon without bundled-modem branding.
- Keep the pinned sotto codec at `e707134`; no Data2G engine is linked.

The bundled Ragtime + Data2G edition continues separately at
https://github.com/N4EAC/ragtime-data2g.

Validation: 57 regression tests passed, including Core Audio device recovery,
voice codec roundtrips, UI and external-host session shutdown. Simulated
Hamlib tuning verified Enter-only manual tuning, preset comments and order,
readback, persistent clearing, and the separate Soft TNC settings page.
