# Upstream fixes surfaced by the web port

Things found while porting the RE Hierarchy Builder to the browser that apply
to the main PAW-Robotics tree. The web version already does the right thing in
each case; the Python and firmware sides do not yet.

Status: `open` / `done` / `wontfix`. Update as you go.

---

## Blocking — generated sketches misbehave or don't compile

### 0. Generated sketches should install a hierarchy, not unroll one — `open`

`games/ethology/codegen.py`, `generate_sketch()`

`EthologyRobot` already owns arbitration. `setHierarchy(const char* const*,
int)` parses wire names against `BEHAVIOR_NAMES`; `hierarchy()` calls
`readSensors()` and walks the rungs through `_runRung()`, which holds every
guard/action pairing and treats cruise as the terminal always-fires case.

The generator instead hand-writes an if/else chain, duplicating `_runRung()`
in a second place that can drift from it — and has, twice (items 1 and 2).

Fix: emit the ordering as data and let the robot execute it.

```cpp
const char* const HIERARCHY[] = { "escape_front", "avoid_object", "cruise_straight" };
const int HIERARCHY_LEN = sizeof(HIERARCHY) / sizeof(HIERARCHY[0]);

void setup() {
  Serial.begin(9600);
  bot.begin(6, 5);
  int bad = bot.setHierarchy(HIERARCHY, HIERARCHY_LEN);
  if (bad != EthologyRobot::HIERARCHY_OK) { /* report index */ }
}

void loop() { bot.hierarchy(); }
```

This is also the path a BLE-delivered hierarchy takes, so a downloaded sketch
and a sent hierarchy become the same thing.

**Items 1 and 2 below become moot for this form** — `readSensors()` runs inside
`hierarchy()`, and cruise ordering stops being a codegen concern. They still
apply to the unrolled form, which is worth keeping as the readable teaching
artifact.

The web version generates both and defaults to this one.

### 1. Generated `hierarchy()` never samples the sensors — `open` (unrolled form only)

`games/ethology/codegen.py`, `generate_sketch()`

The emitted `hierarchy()` is a free function in the sketch, not
`EthologyRobot::hierarchy()`. Nothing refreshes the cached sensor members that
every guard reads, so `proximityThreshold()`, `collisionThreshold()`,
`lightGradientThreshold()` and `backCollisionThreshold()` compare values that
never change.

Fix: emit `bot.readSensors();` as the first statement of `hierarchy()`.

The hand-written `firmware/ethology_standalone/ethology_standalone.ino`
already does this and explains why in a comment — the generator was never
updated to match.

### 2. A misplaced cruise emits `else` followed by `else if` — `open` (unrolled form only)

`codegen.py`, hierarchy body loop

The `elif cond == "true"` branch fires at any position, so a cruise behavior
anywhere but last produces a bare `else` with more `else if` rungs after it.
That is a syntax error; the sketch never compiles.

Fix: a bare `else` only when the cruise is the last entry. Elsewhere emit
`if (true)` (first position) or `else if (true)`. A cruise-only hierarchy is
the lone `if (true)`.

This keeps a nonsensical hierarchy compilable and uploadable, so the dead
rungs are observable on the robot during the lab debrief instead of failing at
the IDE.

### 3. Cached sensor members have no initializer — `done` (in arduino/)

`firmware/shared/EthologyRobot.h` (and the per-sketch copies)

`_leftProxData`, `_rightProxData`, `_lightGradient`, `_leftFrontBumpData`,
`_rightFrontBumpData`, `_leftBackBumpData`, `_rightBackBumpData` are plain
`int` members and `EthologyRobot::EthologyRobot() : Robot() {}` initializes
none of them.

With fix 1 in place this is mostly masked, since `readSensors()` runs before
any guard. It still matters for anything that reads a guard before the first
`readSensors()` call, and it costs nothing to fix.

Fix: default member initializers, or a `readSensors()` call in `begin()`.
Note `collisionThreshold()` treats `0` as *pressed*, so zero-init is not a
safe resting value for the bump members — pick a sentinel that reads as
not-pressed.

### 4. Generated sketches ship without their sources — `open`

`codegen.py` + `hierarchy_builder.py`, `_generate()` / `_submit_hypothesis()`

Both write a lone `.ino` into `sketches/`. It includes `EthologyRobot.h` and
`CogServo.h`, which are project files, not installed libraries, so it will not
compile where it lands. Launch Arduino opens a sketch that immediately errors.

Fix: write a folder named after the sketch containing the `.ino` plus the
`firmware/ethology_standalone/` sources, matching what the web version zips.
Arduino requires the `.ino` basename to equal its folder name.

---

## Correctness — works, but wrong or misleading

### 5. `<Servo.h>` is included twice — `open`

`robots/hardware_profiles.json`, `sketch_includes`

`CogServo.h` includes `<Servo.h>` itself, so every generated sketch carries a
duplicate include.

Fix: empty `sketch_includes` for the bare-Arduino profiles. Keep the field —
a future board may genuinely need an extra library.

### 6. `EthologyRobot.h` points at a constructor that doesn't exist — `done` (in arduino/)

`firmware/shared/EthologyRobot.h`, above the default constructor

The comment says `Servo.h` doesn't list `mbed_giga` among its supported
architectures and advises preferring the PCA9685 constructor. Wrong twice
over:

- There is no PCA9685 constructor. `CogServo.h` records that the whole
  PCA9685 path was removed, and `EthologyRobot()` is the only constructor.
- `Servo.h` does work on the Giga with `writeMicroseconds()`, which is what
  `CogServo` uses. (Confirmed on hardware.)

Fix: delete the comment.

### 7. No Giga hardware profile — `open`

`robots/hardware_profiles.json` defines only `uno_r4_wifi__prototype`, and
`games/ethology/robot.json` points `hardware_profile` at it.

Fix: add `giga_r1_wifi__prototype`, make it the `default`, and repoint
`robot.json`. Field-for-field it matches the R4 entry apart from `label` and
`board`, since pins stay D6/D5.

### 8. Dead fallback key `sketch_bot_decl` — `open`

`codegen.py` still falls back to `sketch_bot_decl` when `sketch_robot_decl` is
absent. A comment in the same function explains that `sketch_bot_decl` passed
a `CogServo` to a constructor that never existed. No profile uses it.

Fix: drop the fallback so a profile missing `sketch_robot_decl` fails loudly.

### 9. Legacy wire aliases — reportedly already fixed — `verify`

`LEGACY_BEHAVIOR_ALIASES` maps `seek_light` → `approach_light` and
`escape_rear` → `escape_back`.

Confirm whether saved student hierarchies still contain the old names. If not,
delete the table and `canonical_behavior()` / `canonical_hierarchy()` from
`codegen.py`, and `LEGACY_BEHAVIOR_ALIASES` / `canonicalBehavior()` from
`index.html`.

---

### 10. Generated sketches never seed the PRNG — `open` (Python side)

`codegen.py` emits no `randomSeed()`. `cruiseArc()` calls `random(2)`, and
Arduino's `random()` repeats an identical sequence after every reset unless
seeded — so a downloaded sketch containing `cruise_arc` makes every robot
produce the same arcs on every power cycle.

The BLE firmware avoids this in `tryInstallHierarchy()` and explains why
`setup()` is the wrong place there: `micros()` at power-on is near-identical
every boot, and the wait for a human to connect is what supplies the entropy.
A standalone sketch has no such wait.

The web version emits, only when the hierarchy contains `cruise_arc`:

```cpp
randomSeed(analogRead(A5) ^ micros());
```

A5 must be left unconnected; A0–A3 carry the sensors. The pin is a profile
field (`sketch_seed_pin`) so a different board can move it.

### 11. Hardware findings from bench testing — `done` (in arduino/)

Four fixes applied to `arduino/ethology_ble_robot/`; fold them into the main
tree.

**`cruise_arc` was indistinguishable from `cruise_straight`.** It flipped a
fresh coin every tick, and a tick is `CRUISE_SECONDS` = 0.1 s — so the
direction changed ten times a second and successive left and right arcs
cancelled. The 30/50 geometry was never the problem; the sampling rate was.
A direction now holds for `ARC_HOLD_MIN_MS`..`ARC_HOLD_MAX_MS` (700–1900 ms)
before re-flipping.

**`leftBackBump` moved from D3 to D8.** D3 is PWM-capable and wanted
elsewhere; a bumper needs only a digital input with a pull-up.

**`avoidLight()` and `approachLight()` were reversed on hardware.** Bodies
swapped. Worth noting *how* this hid: reasoning from the gradient sign gets it
backwards, and the BLE tree had a second reversal in `CogLight` that cancelled
it out. The header comment now says to test against a lamp rather than derive
it.

**`PROX_THRESHOLD` = 35 confirmed good** on hardware. No change; the
calibration item in the checklist can be marked done.

### 12. Display orientation and runtime toggle — `done` (in arduino/)

`CogDisplay::begin()` now calls `setRotation(DISPLAY_ROTATION)` — the panel is
natively 480x800 portrait and the layout assumes 800x480 landscape. Use 3 if
the shield is mounted the other way round.

`setVerbose()` / `toggleVerbose()` / `verbose()` added. `PAW_DISPLAY_DEV`
stays the compile-time gate so a classroom binary cannot be talked into
showing the HUD; the runtime flag switches it within a dev build. Generated
display sketches accept `v` on the serial port to toggle.

### 13. Escape behaviours — `done` (in arduino/)

**`escapeFrontCollision()` had its sides swapped**, so the robot turned into
the obstacle it had just hit and stayed jammed. Verified on hardware; do not
re-derive from the wheel arithmetic.

**The two bumper tests were separate `if`s**, so a square-on hit that closed
both bumpers ran one spin and then the other and they cancelled — the robot
sat still while pinned. They are now exclusive, and a both-sides hit reverses
straight out, which is the only move that clears a head-on obstacle.

**`escapeBackCollision()` ran for 0.1 s** — one tick. Against a robot already
cruising forward that is an imperceptible blip, so `escape_back` looked like
it was not firing. Both escapes now use `ESCAPE_SECONDS` (0.8) so they are
equally legible.

### 14. `approachObject()` used the opposite comparison to its guard — `done`

`proximityThreshold()` defines near as `<= PROX_THRESHOLD`. `approachObject()`
branched on `>= PROX_THRESHOLD`, which tests for *far*.

With an object 20 cm off the right sensor: the guard passed, the right branch
failed (20 is not >= 35), and the left branch fired because the LEFT sensor was
reading 60 cm of empty room — so the robot steered using the side with nothing
in it. The wheel pairs were also away-from rather than toward, compounding it.
Net effect on the floor: approach_object backed away.

Fixed, and a both-sensors-near case added so an object dead ahead is driven
straight at rather than falling through every branch — the exact hole
`EthologyRobot.h` already warns about.

### 15. The BLE sketch had no `PAW_DISPLAY_DEV` define — `done`

Only comments describing it. `CogDisplay.h` supplies a `#ifndef` default of 0,
so the sketch compiled but the HUD could never be switched on from there. A
real define now sits beside `PAW_USE_DISPLAY`, above the include that reads it.

### 16. Build switches must live in a header, not the sketch — `done`

Putting `#define PAW_DISPLAY_DEV` in the `.ino` looked like the discoverable
fix. It is broken: `CogDisplay.cpp` is a separate translation unit and never
sees it, so the sketch declared real methods while the library compiled empty
inline ones. Result:

```
undefined reference to `CogDisplay::setGuards(bool const*, int)'
```

All switches now live in **`PAWConfig.h`**, included by both the sketch and
`CogDisplay.h`, so every translation unit agrees. `CogDisplay.cpp` is wrapped
in `#if PAW_USE_DISPLAY` so a display-free build needs no GFX library at all,
and `CogDisplay.h` supplies the no-op stand-in — the sketch no longer carries
its own copy of that stub.

Verified by linking a two-translation-unit build (sketch + CogDisplay.cpp) at
all three settings, with the values coming only from a stamped `PAWConfig.h`
and no `-D` flags.

## Web-side follow-ups

Not upstream, but the matching to-do here.

- `FIRMWARE_FILES` in `index.html` is a hand-maintained list. Adding a class
  to the dependency closure means adding it there too.
- `canonicalBehavior()` is currently unreachable — nothing loads a saved
  hierarchy. It exists for a future load/save feature. See item 9.
- Transports are unimplemented. When they land they go behind a common
  adapter: `DownloadIno` (ships now), `UsbSerial` (Web Serial), `BleGatt`
  (Web Bluetooth, matching the WinRT GATT path the desktop app uses).
- `arduino/ethology_ble_robot/` carries items 3 and 6 plus the introspection
  API (`lastFiredIndex`, `behaviorAt`, `guardMet`, `snapshot`) and
  `CogDisplay`. Fold these back into the main tree's `firmware/shared/`.
- Two sketch styles ship: hierarchy-object (default) and unrolled if/else.
  The choice persists in `localStorage`. Both were compile-checked with
  `g++ -fsyntax-only -Wall` across valid, cruise-middle, cruise-only and
  all-eight hierarchies.
- No save/load of in-progress hierarchies. If that arrives and should
  interoperate with the desktop app, both need to agree on a format first.
