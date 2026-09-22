# Report for PAW-Robotics-refactor

Everything found while building the browser Hierarchy Builder that belongs in
the main project. Written to be actioned from inside
`PAW-Robotics-refactor`, not from the web repo.

Nothing here has been applied to the main tree. The web repo's
`arduino/ethology_robot_firmware/` holds working versions of every firmware fix, so
the code below can be copied rather than retyped.

**Reference baseline:** the `ethology_ble_robot` firmware as uploaded
2026-08-27. Against it, `Robot`, `CogServo`, `CogAnaDigi`, `CogProximity`,
`CogCollision` and `CogBluetooth` are **unchanged**. All firmware
changes are confined to `EthologyRobot.{h,cpp}`, `CogLight.{h,cpp}`, the
sketch, and three new files.

## Relationship to `PORTING-NOTES.md`

`PORTING-NOTES.md` is the running log kept while building the web app —
chronological, includes web-only work, and preserves superseded reasoning.
This report is the same material reorganised for the main project. Every
upstream item there appears here:

| Notes item | Here |
|---|---|
| 0 install a hierarchy, don't unroll | 4.1 |
| 1 unrolled form never calls `readSensors()` | 4.2 |
| 2 misplaced cruise emits `else` then `else if` | 4.3 |
| 3 cached members uninitialised | 1.7 |
| 4 sketches ship without their sources | 4.4 |
| 5 `<Servo.h>` included twice | 4.6 |
| 6 PCA9685 constructor doesn't exist | 5.1 |
| 7 no Giga hardware profile | 5 |
| 8 dead `sketch_bot_decl` fallback | 4.7 |
| 9 legacy wire aliases | 6.1 |
| 10 no `randomSeed()` | 4.5 |
| 11 bench findings | 1.2, 1.6, 1.8, 1.9 |
| 12 display orientation, verbose toggle | 3.2 |
| 13 escape behaviours | 1.3, 1.4, 1.5 |
| 14 `approachObject()` comparison | 1.1 |
| 15 superseded by 16 | folded into 3.1 |
| 16 build switches must be in a header | 3.1 |
| 17 CogDisplay extras | 3.2 |
| 18 sending over BLE | 6.4 (client-side part only) |
| 19 robot identity used twice | 3.1, 6.4 |

Items 18 and 19 are mostly web-side. What carries upstream is in 6.4.

---

## Contents

- [Part 1 — Behaviour bugs found on hardware](#part-1)
- [Part 2 — EthologyRobot API additions](#part-2)
- [Part 3 — New files](#part-3)
- [Part 4 — codegen.py](#part-4)
- [Part 5 — Hardware profiles](#part-5)
- [Part 6 — Open questions and advisories](#part-6)
- [Appendix — changed constants and pins](#appendix)

---

<a name="part-1"></a>

## Part 1 — Behaviour bugs found on hardware

All were found by running a real robot. Each looked correct in review.

One entry (1.3) records a correction I got *wrong* and then reverted. It is
kept rather than deleted, because the reason it was wrong is the most useful
thing in this section.

### 1.1 `approachObject()` used the opposite comparison to its own guard

**Severity: high — the behaviour did the opposite of its name.**

`proximityThreshold()` defines *near* as `<= PROX_THRESHOLD`. `approachObject()`
branched on `>= PROX_THRESHOLD`, which tests for *far*.

With an object 20 cm off the right sensor: the guard passed, the right branch
failed (20 is not ≥ 35), and the left branch fired because the **left** sensor
was reading 60 cm of empty room. The robot then steered by the side with
nothing in it. The wheel pairs were also away-from rather than toward,
compounding it. On the floor it read as backing away from objects.

```cpp
// BEFORE
void EthologyRobot::approachObject() {
    if (_rightProxData >= PROX_THRESHOLD) { driveProportional(40, 60, 0.1); }
    else if (_leftProxData >= PROX_THRESHOLD) { driveProportional(60, 40, 0.1); }
}

// AFTER
void EthologyRobot::approachObject() {
    const bool right = (_rightProxData <= PROX_THRESHOLD);
    const bool left  = (_leftProxData  <= PROX_THRESHOLD);

    if (right && left) {
        driveProportional(CRUISE_SPEED, CRUISE_SPEED, 0.1);   // dead ahead
    }
    else if (right) { driveProportional(60, 40, 0.1); }
    else if (left)  { driveProportional(40, 60, 0.1); }
}
```

The both-near case is new. Without it, an object dead ahead could satisfy the
guard while every branch missed — the exact hole `EthologyRobot.h` already
warns about in its `hierarchy()` comment.

### 1.2 Light behaviours were reversed

**Severity: high.** `avoidLight()` and `approachLight()` had their bodies
swapped relative to what the hardware does. The two functions are exact
mirrors, so the fix is to exchange them.

Worth recording *how this hid*: reasoning from the sign of the gradient gets it
backwards, and the pre-refactor tree had a second reversal in `CogLight` that
cancelled it out. The corrected code now carries a comment saying to test
against a lamp rather than re-derive it. **Keep that comment when merging.**

### 1.3 `escapeFrontCollision()` per-side directions — NO CHANGE

**This entry previously claimed the sides were swapped. That was wrong.** The
original pairings are correct and hardware-confirmed: a left hit spins
`(100, -100)`, a right hit spins `(-100, 100)`. An intermediate version of
this report had them exchanged, which drove the robot into whatever it had
just hit.

Recorded because the mistake is instructive: the error came from assuming
these shared the fault found in the light behaviours (1.2). They do not. The
light behaviours are reversed relative to what the wheel arithmetic suggests;
these are not. **Neither can be derived — both must be observed.**

The exclusive-branch fix in 1.4 is real and independent of this.

### 1.4 `escapeFrontCollision()` cancelled itself on a head-on hit

**Severity: high, and independent of 1.3.** The two bumper tests were separate
`if` statements, so a square-on hit closing both bumpers ran one spin and then
the other. They cancelled: the robot sat still while pinned, which is the worst
available response to being stuck.

1.4 fixed, with 1.3's directions left as they were:

```cpp
// AFTER
void EthologyRobot::escapeFrontCollision() {
    const bool left  = (_leftFrontBumpData  == 0);
    const bool right = (_rightFrontBumpData == 0);

    if (left && right) { driveProportional(-100, -100, ESCAPE_SECONDS); } // back out
    else if (left)     { driveProportional( 100, -100, ESCAPE_SECONDS); }
    else if (right)    { driveProportional(-100,  100, ESCAPE_SECONDS); }
}
```

### 1.5 `escapeBackCollision()` was too brief to observe

**Severity: medium.** It ran for 0.1 s — one tick. Against a robot already
cruising forward that is an imperceptible blip, so `escape_back` looked like it
was not firing even when its guard was met.

Both escapes now use `ESCAPE_SECONDS = 0.8` so they are equally legible.

### 1.6 `cruise_arc` was indistinguishable from `cruise_straight`

**Severity: medium.** It flipped a fresh coin every tick, and a tick is
`CRUISE_SECONDS` = 0.1 s. The direction changed ten times a second and
successive left and right arcs cancelled, so the robot tracked essentially
straight with a wobble.

The 30/50 arc geometry was never the problem — the sampling rate was. A
direction now holds for a random 700–1900 ms before re-flipping:

```cpp
void EthologyRobot::cruiseArc() {
    const unsigned long now = millis();
    if (now >= _arcUntilMs) {
        _arcLeft    = (random(2) == 0);
        _arcUntilMs = now + random(ARC_HOLD_MIN_MS, ARC_HOLD_MAX_MS + 1);
    }
    if (_arcLeft) { cruiseLeftArc(); } else { cruiseRightArc(); }
}
```

Needs `_arcLeft` and `_arcUntilMs` members, and still requires `randomSeed()`
— see 4.5.

### 1.7 Cached sensor members had no initialiser

**Severity: medium.** `int _leftProxData;` and the other six had no member
initialiser and an empty constructor init list, so they held indeterminate
values until the first `readSensors()`.

Zero is not a safe resting value: `collisionThreshold()` treats 0 as *pressed*
and `proximityThreshold()` treats low as *near*. They are now initialised to a
resting world — see the [Appendix](#appendix).

### 1.8 `leftBackBump` was on a PWM pin

D3 is PWM-capable and wanted elsewhere; a bumper needs only a digital input
with a pull-up. Moved to **D8**. `rightBackBump` stays on D7.

Note the prototype still has no back bumpers physically wired, so
`escape_back` remains untested on hardware.

### 1.9 `PROX_THRESHOLD` confirmed

35 verified good on hardware. No change. The mapping still saturates at 18 cm
(a wall at 18 cm and one at 2 cm read the same), which matters only if the
threshold is ever raised.

---

### 1.10 Tuning from hardware, and escapes that arc backward

**Constants.** `LIGHT_THRESHOLD` 15 → 20 → 25, `CRUISE_SPEED` 60 → 80, arc
30/50 → 50/70. The arc keeps a fixed 20-point wheel difference below cruise, so
its radius grows only with the speed sum — about 0.16 m → 0.24 m by the
header's own figures, well short of the 0.52 m the header warns about.

`LIGHT_THRESHOLD` stays an absolute value on purpose: sensitivity then depends
on sensor geometry (splayed or spaced wider sees a steeper gradient), so robots
built differently respond differently, which is part of what students observe.

**Front-collision escapes now arc backward** instead of spinning in place,
duration unchanged. Spinning changed heading without moving the robot, so it
could rotate clear and drive straight back in.

```cpp
if (left && right) { driveProportional(-100, 100, ESCAPE_SECONDS * 2); }
else if (left)     { driveProportional( -40, -80, ESCAPE_SECONDS); }
else if (right)    { driveProportional( -80, -40, ESCAPE_SECONDS); }
```

A head-on hit that pins both bumpers spins rather than reversing straight out:
reversing left the robot still facing the obstacle, so it drove back into it.

**Verified on hardware — and the first values were wrong.** They were
initially set the other way round. On the floor that turned the robot *into*
the struck side, and with `avoid_object` in the same hierarchy the two fought:
the escape backed toward the obstacle, avoidance steered off it, repeat.

The wrong values had been checked by comparing wheel-difference signs against
the old hardware-verified spin pairings. That is a relative argument, not
first-principles arithmetic, and it still gave the wrong answer — backing up
while turning does not read like spinning in place. So the rule for this
codebase is absolute: no reasoning, relative or otherwise, substitutes for
pressing a bumper.

---

### 1.11 Two light-sensor types read opposite ways round

Two kinds of light sensor are in use. On one, the raw `analogRead` value rises
as light gets dimmer; on the other it rises as light gets brighter.
`CogLight::getData()` hard-coded `map(raw, 0, 1023, 100, 0)` — correct only for
the first.

Every light behaviour assumes `getData()` means 0 = dark, 100 = bright. On the
wrong sensor the gradient's sign flips, so `approach_light` flees the lamp and
`avoid_light` chases it — which looks exactly like a behaviour bug and invites
someone to "fix" the behaviours instead of the mapping.

New define in `PAWConfig.h`, which `CogLight.cpp` now includes:

```cpp
// 0 = raw HIGH means DARK   -> map(raw, 0, 1023, 100, 0)   (default)
// 1 = raw HIGH means BRIGHT -> map(raw, 0, 1023, 0, 100)
#define PAW_LIGHT_HIGH_IS_BRIGHT 0
```

The default reproduces the old behaviour exactly, since it is the sensor the
light behaviours were tuned on. Verified by driving `CogLight` from a
controllable `analogRead`: at 0, raw 0 → 100 and raw 1023 → 0; at 1, the
reverse.

It must be a header, for the same reason as item 3.1: `CogLight.cpp` is its own
translation unit and would never see a define placed in the sketch.

---

<a name="part-2"></a>

## Part 2 — `EthologyRobot` API additions

Four additions. None affect arbitration; all expose state the tick already
produces. They exist for the display HUD but are equally useful for
`PAW_SENSOR_TRACE` and the simulation.

### 2.1 `guardMet(Behavior)` — the important one

`_runRung()` was eight `if (guard) { act; return true; }` pairs, which meant
the guard-to-behaviour mapping existed there and nowhere else. Nothing could
ask *would this rung fire?* without acting on the answer.

Split the test out; `_runRung()` then calls it:

```cpp
bool EthologyRobot::guardMet(Behavior b) {
    switch (b) {
        case Behavior::EscapeFront:    return collisionThreshold();
        case Behavior::EscapeBack:     return backCollisionThreshold();
        case Behavior::AvoidObject:
        case Behavior::ApproachObject: return proximityThreshold();
        case Behavior::AvoidLight:
        case Behavior::ApproachLight:  return lightGradientThreshold();
        case Behavior::CruiseStraight:
        case Behavior::CruiseArc:      return true;
        default:                       return false;
    }
}

bool EthologyRobot::_runRung(Behavior b) {
    if (!guardMet(b)) return false;
    switch (b) {
        case Behavior::EscapeFront:    escapeFrontCollision(); return true;
        // ...one line per behaviour...
        default:                       return false;
    }
}
```

Safe because the guards read only values cached by `readSensors()`, so asking
twice within a tick cannot disagree with what arbitration used.

**This is now the single source of the mapping. Do not reintroduce a second
copy** — that duplication is how 1.1 and 1.2 both survived review.

### 2.2 `lastFiredIndex()` and `behaviorAt(int)`

`hierarchy()` records which rung won:

```cpp
void EthologyRobot::hierarchy() {
    readSensors();
    _lastFired = -1;
    for (int i = 0; i < _hierLen; i++) {
        if (_runRung(_hier[i])) { _lastFired = i; return; }
    }
}
```

`_lastFired` also resets in `clearHierarchy()` and on a successful
`setHierarchy()`, so a stale index cannot outlive the hierarchy it indexed.

### 2.3 `snapshot()`

Returns the seven cached readings this tick's decision was made on.

Do **not** re-read the Cog objects instead: `CogCollision` has no `peekData()`,
so a fresh `getData()` takes a *new* sample that may disagree with the one
arbitration used. The commented-out display block in the old sketch did exactly
this.

### 2.4 Why it matters beyond the display

`hierarchy()` short-circuits at the first rung that fires, so it never learns
whether the rungs below would also have passed. Evaluating them separately is
what distinguishes "correctly subsumed" from "broken" — and on a partly wired
robot a floating pin holds a guard true and starves everything below it, a
failure that from outside looks identical to "the robot just sits there".

---

<a name="part-3"></a>

## Part 3 — New files for `firmware/shared/`

### 3.1 `PAWConfig.h` — required, and the reason the others work

Holds `PAW_ROBOT_ID`, `PAW_USE_DISPLAY`, `PAW_DISPLAY_DEV`, `PAW_SENSOR_TRACE`,
and derives `PAW_ROBOT_NAME`.

**This must be a header, not defines in the `.ino`.** `CogDisplay.cpp` is a
separate translation unit and never sees a sketch-level `#define`, so the
sketch declares real methods while the library compiles empty inline ones and
the link fails with:

```
undefined reference to `CogDisplay::setGuards(bool const*, int)'
```

This was hit for real. Putting the switches in the sketch looks like the
obvious, discoverable fix every single time. It is wrong.

### 3.2 `CogDisplay.h` / `.cpp` — optional

GIGA Display Shield output, in two tiers:

- `PAW_DISPLAY_DEV 0` — BLE status and session only. Classroom setting.
- `PAW_DISPLAY_DEV 1` — adds the rung list with a lit dot per rung whose guard
  is met, a `SUBSUMED` marker, and a raw sensor strip.

Uses **`Arduino_GigaDisplay_GFX`**, not `Arduino_H7_Video` as the old sketch
comment claimed — that is the lvgl stack, far heavier than needed for text and
rectangles. Requires GIGA core 4.0.6+.

Notes for merging:
- `begin()` calls `setRotation(1)`; the panel is natively 480×800 portrait and
  the layout is 800×480 landscape.
- The whole class is gated on `PAW_USE_DISPLAY`, and `CogDisplay.h` supplies
  its own no-op stand-in, so the sketch does not need to carry a stub.
- `setVerbose()` / `toggleVerbose()` give a runtime toggle within a dev build.
  `PAW_DISPLAY_DEV` stays the compile-time gate so a classroom binary cannot be
  talked into revealing the hierarchy.
- `setStatus()` is an inline alias for `setBleStatus()`, for standalone
  sketches with no BLE link.

### 3.3 Sketch changes

`ethology_robot_firmware.ino`: the config block is replaced by an
`#include "PAWConfig.h"`, its private `CogDisplay` stub is deleted, and the
commented-out display block becomes live calls sourced from `snapshot()` and
`guardMet()` rather than from re-reading Cogs.

One ordering trap: call `display.setHierarchy(ble.pendingNames(), count)`
**before** `ble.acceptHierarchy()`, which calls `_clearPending()`.

---

<a name="part-4"></a>

## Part 4 — `games/ethology/codegen.py`

Nothing here has been fixed in the main tree. The web app's two generators
are working references: the downloaded sketch for the unrolled form (4.2, 4.3,
4.5), and the BLE receiver's install path for the hierarchy-object form (4.1).

### 4.1 Generated sketches should install a hierarchy, not unroll one

`generate_sketch()` hand-writes an if/else chain, duplicating `_runRung()` in a
second place that can drift from it — and has, twice. `EthologyRobot` already
owns arbitration.

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
and a sent hierarchy become the same object. Fixing this makes 4.2 and 4.3
moot for the generated sketch.

### 4.2 The unrolled form never calls `readSensors()`

Only relevant if the unrolled style is kept as a teaching artifact. The
generated `hierarchy()` is a free function, not `EthologyRobot::hierarchy()`,
so nothing refreshes the cached members every guard reads. Emit
`bot.readSensors();` as its first statement.

### 4.3 A misplaced cruise emits `else` followed by `else if`

The `elif cond == "true"` branch fires at any position, so a cruise anywhere
but last produces a bare `else` with more `else if` rungs after it. That is a
syntax error; the sketch never compiles.

A bare `else` only when the cruise is last. Elsewhere `if (true)` or
`else if (true)`, which compiles and makes the dead rungs observable on the
robot — the intended lab-debrief artifact.

### 4.4 Generated sketches ship without their sources

Both `_generate()` and `_submit_hypothesis()` write a lone `.ino` into
`sketches/`. It includes `EthologyRobot.h` and `CogServo.h`, which are project
files rather than installed libraries, so it cannot compile where it lands and
"Launch Arduino" opens something that immediately errors.

Write a **folder** named after the sketch, containing the `.ino` plus the
firmware sources. Arduino requires the `.ino` basename to equal its folder
name. The web version zips 14 class files alongside the sketch.

### 4.5 No `randomSeed()`

`cruiseArc()` calls `random(2)`, and Arduino's `random()` repeats an identical
sequence after every reset unless seeded — so every robot in the room produces
the same arcs on every power cycle.

The BLE firmware seeds in `tryInstallHierarchy()` and explains why `setup()` is
the wrong place *there*: `micros()` at power-on is near-identical every boot,
and the wait for a human to connect supplies the entropy. A standalone sketch
has no such wait, so the web version emits, only when `cruise_arc` is present:

```cpp
randomSeed(analogRead(A5) ^ micros());
```

A5 must be left unconnected; A0–A3 carry the sensors.

### 4.6 `<Servo.h>` included twice

`sketch_includes` lists it and `CogServo.h` includes it already. Empty
`sketch_includes` for the bare-Arduino profiles; keep the field for a future
board that genuinely needs an extra library.

### 4.7 Dead `sketch_bot_decl` fallback

`codegen.py` still falls back to `sketch_bot_decl` when `sketch_robot_decl` is
absent, and a comment in the same function explains that `sketch_bot_decl`
passed a `CogServo` to a constructor that never existed. No profile uses it.
Drop the fallback so a profile missing `sketch_robot_decl` fails loudly.

---

<a name="part-5"></a>

## Part 5 — `robots/hardware_profiles.json` and `robot.json`

Only `uno_r4_wifi__prototype` is defined, and `robot.json` points at it. Add a
Giga entry and make it the default:

```json
"giga_r1_wifi__prototype": {
  "label": "Giga R1 WiFi — Prototype (bare Arduino)",
  "board": "giga_r1_wifi",
  "shield": "prototype",
  "servo_init": "servo_h",
  "pin_namespace": "gpio",
  "sketch_includes": [],
  "sketch_servo_decl": "",
  "sketch_servo_begin": "",
  "sketch_robot_decl": "EthologyRobot bot;",
  "sketch_bot_begin": "bot.begin({left_pin}, {right_pin});"
}
```

Field-for-field identical to the R4 entry apart from `label` and `board`; pins
stay D6/D5.

### 5.1 The `Servo.h` / Giga comment is wrong twice over

`EthologyRobot.h` claimed `Servo.h` does not list `mbed_giga` among its
architectures and advised preferring the PCA9685 constructor.

- There is no PCA9685 constructor. `CogServo.h` records that the path was
  removed.
- `Servo.h` **does** work on the Giga via `writeMicroseconds()`, which is what
  `CogServo` uses. Confirmed on hardware.

The comment is deleted in the web repo's copy. Delete it in the main tree too —
it sends people after something that does not exist.

The IDE still emits an architecture warning for `Servo`. Harmless. Note also
that a user-installed `Servo` and `Arduino_H7_Video` under
`Documents\Arduino\libraries` will shadow the versions bundled with GIGA core
4.6.0; deleting the user copies lets the core's win.

---

<a name="part-6"></a>

## Part 6 — Open questions and advisories

### 6.1 Legacy wire names — needs a decision

`LEGACY_BEHAVIOR_ALIASES` maps `seek_light` → `approach_light` and
`escape_rear` → `escape_back`. `EthologyRobot.h` states the Python side still
sends the old names.

This has teeth: `setHierarchy()` rejects a hierarchy **whole** on an
unrecognised name — it does not drop the bad rung and run the rest. So any
saved hierarchy still containing the old names is silently unusable, and the
robot simply does nothing.

Either confirm no live saved hierarchy contains them and delete the alias table
everywhere, or confirm the translation runs on every load path. The web version
carries `canonicalBehavior()` but it is currently unreachable there — nothing
loads a saved hierarchy yet.

### 6.2 Back bumpers

D7 and D8, but the prototype has none physically wired. `escape_back` is
untested. Either wire them or drop `escape_back` from classroom hierarchies.

### 6.3 Wire-name vocabulary is duplicated in four places

`BEHAVIOR_NAMES` in `EthologyRobot.cpp` is the stated authority, and
`behaviorCatalog()` exists so a host can discover it. But the same eight
strings are hardcoded in `codegen.py`, in `robot_bt_client.py`, and now in the
web app's `index.html`.

Nothing to fix urgently, but worth knowing that adding a ninth behaviour means
four edits, and the `static_assert` only catches the C++ half.

---

### 6.4 Two protocol details for `robot_bt_client.py`

Implementing the same handshake in the browser surfaced two things that are
easy to get wrong and produce failures that look like dead hardware. Worth
checking the Python client against them; no change is known to be needed.

**Subscribe to the DATA characteristic before writing to CMD.** The robot's
reply can otherwise land before the listener is attached, and the client waits
out its timeout against a robot that already answered.

**A successful write is not success.** `{"cmd":"run"}` only *stages* the
names — `CogBluetooth` hands them to `EthologyRobot::setHierarchy()`, which
validates against `BEHAVIOR_NAMES` and can reject. The robot then answers
`running`, `busy`, or an error on DATA. Treating the write as the outcome hides
a rejected behaviour name, and since `setHierarchy()` rejects a hierarchy
*whole* (see 6.1), the robot sits there doing nothing with no indication why.

Related: the three service UUIDs are now duplicated in three places —
`CogBluetooth.h`, `robot_bt_client.py`, and the web app's `index.html`. Each
carries a comment saying the others must match.

---

<a name="appendix"></a>

## Appendix — changed constants and pins

### New constants in `EthologyRobot.h`

| Constant | Value | Why |
|---|---|---|
| `ESCAPE_SECONDS` | `1.2` | Both escapes; 0.1 was imperceptible (1.5) |
| `LIGHT_THRESHOLD` | `25` (was 15) | Raised in steps on hardware (1.10) |
| `CRUISE_SPEED` | `80` (was 60) | Reads better; stronger collisions (1.10) |
| `ARC_INNER_SPEED` | `50` (was 30) | Set on hardware (1.10) |
| `ARC_OUTER_SPEED` | `70` (was 50) | Set on hardware (1.10) |
| `ARC_HOLD_MIN_MS` | `700` | Lower bound on one arc direction (1.6) |
| `ARC_HOLD_MAX_MS` | `1900` | Upper bound |

### Cached sensor initialisers

| Member | Was | Now | Why |
|---|---|---|---|
| `_leftProxData` | uninitialised | `999` | Far; guard wants `<= 35` |
| `_rightProxData` | uninitialised | `999` | |
| `_lightGradient` | uninitialised | `0` | Guard needs `\|g\| >= 15` |
| `_leftFrontBumpData` | uninitialised | `1` | HIGH = not pressed |
| `_rightFrontBumpData` | uninitialised | `1` | |
| `_leftBackBumpData` | uninitialised | `1` | |
| `_rightBackBumpData` | uninitialised | `1` | |

### New members

`int _lastFired = -1;`, `bool _arcLeft = false;`,
`unsigned long _arcUntilMs = 0;`

### Pin map as built

| Function | Pin | Note |
|---|---|---|
| Left servo | D6 | |
| Right servo | D5 | |
| Right front bumper | D2 | |
| Left front bumper | D4 | |
| Right back bumper | D7 | not physically wired |
| Left back bumper | **D8** | was D3, which is PWM |
| Left proximity | A0 | |
| Right proximity | A1 | |
| Left light | A2 | |
| Right light | A3 | |
| PRNG seed | A5 | must be left unconnected |

### Files touched, against the 2026-08-27 baseline

| File | Status |
|---|---|
| `EthologyRobot.h` | modified — Parts 1, 2, Appendix |
| `EthologyRobot.cpp` | modified — Parts 1, 2 |
| `ethology_robot_firmware.ino` | modified — Part 3.3 |
| `PAWConfig.h` | **new** |
| `CogDisplay.h` / `.cpp` | **new** |
| `CogLight.h` / `.cpp` | modified — 1.11 |
| `Robot`, `CogServo`, `CogAnaDigi`, `CogProximity`, `CogCollision`, `CogBluetooth` | unchanged |
