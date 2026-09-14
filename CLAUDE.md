# Working on this repo

A browser Hierarchy Builder for a subsumption-architecture teaching robot.
Students arrange behaviours into a priority hierarchy and either download an
Arduino sketch or send it to a running robot over Bluetooth.

Run `./tools/check.sh` before and after any change. It links the firmware at
every display setting, parses the web app, and verifies the bundled firmware
copy is in sync. No Arduino IDE or robot needed.

---

## Layout

```
index.html                    the entire web app — vanilla JS, no build step
firmware/ethology/            14 files bundled into every student download
firmware/display/             CogDisplay + PAWConfig, bundled when display is on
firmware/ble/                 CogBluetooth + the receiver sketch
arduino/ethology_ble_robot/   the robot firmware — open THIS in the Arduino IDE
tools/check.sh                verification; tools/stubs/ backs it
sync-firmware.sh              arduino/ -> firmware/ethology/
MAIN-PROJECT-REPORT.md        what to change in PAW-Robotics-refactor, with code
PORTING-NOTES.md              running log, 19 items; "At a glance" first
CHECKLIST.md                  hardware bring-up, staged
```

`index.html` is one file on purpose: it drops into a GitHub Pages repo with no
build step, no npm, no CI. Do not introduce a bundler.

## Two different sketches

Confusing these wastes time, so check which one is meant.

| | What it is |
|---|---|
| **Student sketch** | Downloaded from the web app. A `HIERARCHY[]` array, `setHierarchy()`, `loop()`. No BLE, no heartbeat. |
| **Robot firmware** | `arduino/ethology_ble_robot/`. Receives hierarchies over BLE, heartbeat LED, `CogDisplay`. |

`firmware/ethology/` is a **copy** of the class files from
`arduino/ethology_ble_robot/`. After changing firmware, run
`./sync-firmware.sh arduino` or downloads ship stale sources — and they compile
cleanly, so nothing warns you.

---

## Invariants

**Build switches live in `PAWConfig.h`, never in a `.ino`.** `CogDisplay.cpp`
is a separate translation unit; a `#define` in the sketch does not reach it, so
the sketch declares real methods while the library compiles empty inline ones
and the link fails with `undefined reference to CogDisplay::setGuards`. This
mistake has been made once already and looks like the obvious fix every time.
`tools/check.sh` catches it.

**Wire names are the contract.** `BEHAVIOR_NAMES` in `EthologyRobot.cpp` is the
authority. `setHierarchy()` rejects a hierarchy *whole* on an unknown name — it
does not degrade — so a mismatch makes the behaviour unusable rather than
partly working. The same eight strings appear in `index.html`, in
`codegen.py` in the main Python project, and in `robot_bt_client.py`.

**Guard-to-behaviour mapping lives only in `EthologyRobot::guardMet()`.**
`_runRung()` calls it. Do not reintroduce a second copy; that duplication is
how two separate bugs survived.

**Never bundle `CogBluetooth` or `CogDisplay` into a student download by
default.** They would make every download depend on ArduinoBLE and
Arduino_GigaDisplay_GFX. `CogDisplay` ships only when the display setting is on.

**The classroom display tier shows status only.** Students infer the hierarchy
from watching the robot; showing the rung list gives away the exercise.
`PAW_DISPLAY_DEV 0` compiles the HUD out entirely, so a classroom binary cannot
be talked into revealing it.

**Do not derive motor polarity from wheel arithmetic.** The light behaviours,
`escapeFrontCollision()` and `approachObject()` were each wrong in a way that
looked correct on paper. Every one was fixed from hardware observation, and the
comments say so. If a behaviour seems backwards, test it; do not reason it out
and "correct" the code.

---

## Verifying without hardware

`tools/check.sh` links a real two-translation-unit build against stub headers in
`tools/stubs/`. Those stubs declare only what this firmware uses.

If a check fails with "no member named X", the stub is behind the real Arduino
API — **add the signature to the stub**, do not change the firmware to avoid it.

It cannot catch board-specific behaviour, timing, or anything about servos.
`CHECKLIST.md` covers what genuinely needs the robot.

To exercise the web app, serve it — `fetch` has no origin on `file://` and the
firmware bundle will not load:

```bash
python3 -m http.server 8000    # then http://localhost:8000
```

---

## Related repos

The main **PAW-Robotics** Python project is separate and not in this repo. It
holds the desktop builder (`games/ethology/codegen.py`), the simulation, and
`firmware/shared/`. `PORTING-NOTES.md` lists what needs folding back into it;
`arduino/ethology_ble_robot/` is currently well ahead of `firmware/shared/`.

If asked to fix something in `codegen.py`, note that it is not here.
`MAIN-PROJECT-REPORT.md` describes every change with the code to copy, but
applying it needs `PAW-Robotics-refactor` open.

---

## Style

Comments explain *why*, especially where a change reverses something that looks
correct. Several comments in this codebase exist specifically to stop a future
reader from "fixing" a hard-won polarity back to the broken version. Preserve
them; when fixing a bug of that kind, add one.

The web app has no dependencies and no build step. Keep it that way.
