# Changelog

Notable changes to the Hierarchy Builder, newest first. Format follows Keep a
Changelog (keepachangelog.com); version numbers follow the rules in
`VERSIONING.md`.

## [Unreleased]

## [1.0.0] — 2026-09-21

First versioned release. Everything before this point is folded in here.

### Workflows

- **BLE receiver sketch** — flash once per robot; no hierarchy baked in.
- **Send over BLE** — sends the current hierarchy to the selected robot, and
  waits for the robot's own accept or reject rather than treating a successful
  write as success.
- **Download sketch folder** — the hierarchy written out as if/else rungs in
  `loop()`, with every firmware source it needs. No Bluetooth.

### Settings

- Robot letter (A–D), stamped as `PAW_ROBOT_ID`; the same letter filters the
  Bluetooth scan.
- Display: Off / Status / Full HUD. Full HUD applies to Send over BLE only.
- Light sensor: High = dark (default) / High = bright, stamped as
  `PAW_LIGHT_HIGH_IS_BRIGHT`.
- Board: Giga (default) / Uno R4.
- Four colour themes.

### Firmware, as shipped in downloads

- Cruise speed 80; arc 50/70, holding one direction for 0.7–1.9 s.
- Light threshold 25.
- Front collision arcs backward away from the struck side; a head-on hit
  reverses straight out.
- Left back bumper on D8.
- `approachObject()` corrected to steer toward, not away from, an object.

### Known issues

- Send over BLE does not pair on macOS. Firefox and Safari do not implement
  Web Bluetooth; Chrome is under investigation. See `PORTING-NOTES.md`.
- Back bumpers are not physically wired on the prototype, so `escape_back` is
  untested on hardware.
