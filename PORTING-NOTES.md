# Upstream fixes surfaced by the web port

> **Working in PAW-Robotics-refactor?** Read
> [`MAIN-PROJECT-REPORT.md`](MAIN-PROJECT-REPORT.md) instead. It covers the
> same ground organised for that repo, with the code to copy, and without the
> web-side items. This file is the running log and keeps the history.

Things found while porting the RE Hierarchy Builder to the browser that apply
to the main PAW-Robotics tree. The web version already does the right thing in
each case; the Python and firmware sides do not yet.

Status: `open` / `done` / `wontfix`. Update as you go.

## At a glance

**Still open, Python side** — items 0, 1, 2, 4, 5, 7, 8, 10. All concern
`codegen.py` and the desktop builder; none block the web app, which already
does the right thing in each case.

**Needs an answer from you** — item 9 (are `seek_light` / `escape_rear` still
in live saved hierarchies?). This one has teeth: `setHierarchy()` rejects a
hierarchy *whole* on an unknown name, so any file containing them is silently
unusable.

**Done, but only in `arduino/ethology_ble_robot/`** — items 3, 6, 11, 12, 13,
14, 16, 17, 19. That folder is ahead of the main tree; see the fold-back list
at the end.

**Untested on hardware** — `escape_back` (back bumpers are not physically
wired), and sending over Bluetooth.

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
(in `PAWConfig.h`, see item 16) stays the compile-time gate so a classroom
binary cannot be talked into showing the HUD; the runtime flag switches it
within a dev build. Generated display sketches accept `v` on the serial port
to toggle.

### 13. Escape behaviours — `done` (in arduino/)

**`escapeFrontCollision()` per-side directions: reverted, no change.** This
was briefly "fixed" by exchanging them, which was wrong — the originals are
correct and hardware-confirmed (left `(100, -100)`, right `(-100, 100)`). The
error came from assuming they shared the light-behaviour fault; they do not.
The light behaviours are reversed relative to the wheel arithmetic and these
are not, which is why neither can be derived.

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

### 15. The BLE sketch had no `PAW_DISPLAY_DEV` define — `superseded by 16`

Only comments described it, so the HUD could never be switched on from the
sketch. The first fix added a real define to the `.ino`. **That fix was wrong**
— see item 16. Both defines now live in `PAWConfig.h` and the `.ino` has
none of its own. Kept here only so the reasoning is not repeated.

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

### 17. CogDisplay additions beyond the HUD — `done` (in arduino/)

- `setStatus(status, detail)` as an inline alias for `setBleStatus()`. A
  downloaded standalone sketch has no BLE link, and calling `setBleStatus` in
  one reads as a mistake.
- `setRotation(DISPLAY_ROTATION)` in `begin()`; the panel is natively 480x800
  portrait and the layout is 800x480 landscape. Use 3 if mounted inverted.
- The `PAW ROBOTICS` title row was removed and the layout reflowed upward:
  `STATUS_Y` 26→10, `DETAIL_Y` 100→84, `RULE1_Y` 130→114, `LIST_Y` 140→126.
  The rung list gained the reclaimed height.

### 18. Sending hierarchies over BLE — `web only, nothing upstream`

The web app now talks the protocol in `CogBluetooth.h` directly via Web
Bluetooth, so a hierarchy can go to a running robot without the Arduino IDE.
No firmware change was needed — the protocol was already complete.

Two details worth knowing if the Python client is ever revisited, because
both are easy to get wrong:

- **Subscribe to DATA before writing CMD.** The reply can otherwise land
  before the listener is attached.
- **A successful write is not success.** `{"cmd":"run"}` only stages the
  names; the robot validates them and answers `running`, `busy` or an error.
  Treating the write as the outcome hides a rejected behaviour name, which is
  exactly the failure that looks like a dead robot.

`RobotBLEClient` in `engine/bluetooth/robot_bt_client.py` must keep the same
three UUIDs. They are duplicated in `index.html` with a comment saying so.

### 19. Robot identity is one setting, used twice — `done`

`PAW_ROBOT_ID` in `PAWConfig.h` sets the advertising name, and the web app
stamps it when you download a receiver sketch. The same picker filters the
scan when sending. Splitting these into two settings would recreate the fault
`CogBluetooth.h` already documents: boards flashed from an unedited copy all
came up as `RobotA`.

## Web-side follow-ups

Not upstream, but the matching to-do here.

- `FIRMWARE_FILES` in `index.html` is a hand-maintained list. Adding a class
  to the dependency closure means adding it there too.
- `canonicalBehavior()` is currently unreachable — nothing loads a saved
  hierarchy. It exists for a future load/save feature. See item 9.
- Transports: download and Web Bluetooth both ship. Web Serial over USB is
  still unimplemented — it would work on every board, including ones with no
  radio, and is the obvious next one.
- **`arduino/ethology_ble_robot/` is ahead of the main tree.** Everything
  marked `done` above lives there and nowhere else. What to fold back into
  `firmware/shared/`:
  - `EthologyRobot` — member initialisers, `lastFiredIndex()`,
    `behaviorAt()`, `guardMet()` + the `_runRung()` refactor, `snapshot()`,
    the `cruiseArc` hold, `ESCAPE_SECONDS`, the fixed `escapeFrontCollision()`
    / `escapeBackCollision()` / `approachObject()` / light behaviours, and
    `leftBackBump` on D8.
  - `CogDisplay.h` / `.cpp` — new files.
  - `PAWConfig.h` — new file, and the reason the build switches work at all.
  - `ethology_ble_robot.ino` — config block replaced by the `PAWConfig.h`
    include; its private `CogDisplay` stub deleted, since the header now
    supplies one.
- Display mode (Off / Status / Full HUD) and robot letter are UI settings
  stamped into `PAWConfig.h` at download time, for both the hierarchy sketch
  and the BLE receiver.
- Two sketch styles ship: hierarchy-object (default) and unrolled if/else.
  The choice persists in `localStorage`. Both were compile-checked with
  `g++ -fsyntax-only -Wall` across valid, cruise-middle, cruise-only and
  all-eight hierarchies.
- No save/load of in-progress hierarchies. If that arrives and should
  interoperate with the desktop app, both need to agree on a format first.
