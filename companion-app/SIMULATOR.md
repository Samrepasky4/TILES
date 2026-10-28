# Companion app simulator

Start from `companion-app/` with `TILES_MOCK=1 npm run dev` (macOS/Linux).
The simulator runs without any physical instrument.

The mock USB command handler is in `src/main/mockUsb.ts`. Its firmware 0.3.6
setting definitions come from `fixtures/tiles-settings.json`, based on the
official `shared/protocol/README.md` catalog. It is a **simulation**, not a
hardware test or a real firmware image.

- Firmware 0.3.6 starts with RGB scheme colors, 24 per-pad override settings,
  and 9 editable custom scale slots.
- The header's simulator-only firmware selector switches to the 0.2.0
  compatibility fixture; this resets the mock device. `SIM VERSION` is
  intentionally *not* a real firmware command.
- `GET`, `SET`, `LIST`, `SCHEMA`, `INFO`, `RESET`, `SAVE`,
  `SCALES`, `SCALE GET/PUT/DELETE`, and `CONTENT LIST/CLEAR` are
  interpreted locally. Responses use the same format as the renderer's USB
  adapter: multiline terminal `OK` is removed.
- The simulator validates documented setting ranges, values, RGB colors,
  and custom scale slot/interval syntax.
- `TEST` mode is only acknowledged for a subset of commands; no real
  electrical/haptic/MIDI effect can be reproduced.
- Simulator writes remain in memory until the Electron main process ends.
  Physical-device persistence and USB transfers are **not** tested.
- The visualizer approximates how role and pad colors change with a chosen
  scale. Its scale dropdown is **preview only**: the current USB protocol
  uploads scales but does not offer a command to select an active scale.

The real USB path (`src/main/usb.ts`) remains separate from this simulator.
For verification on an actual board, compare `INFO`, `SCHEMA`,
`LIST`, `SCALES`, and a `SCALE PUT/GET/DELETE` round trip.
