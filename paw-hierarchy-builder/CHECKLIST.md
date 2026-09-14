# Bring-up checklist

Everything needed is in this repo. No copying from the Python project.

## Two different sketches

Read this first — the stages below alternate between them.

| | Where | What it is |
|---|---|---|
| **Student sketch** | downloaded from the web app | Standalone. A `HIERARCHY[]` array, `setHierarchy()`, `loop()`. No BLE, no heartbeat LED. Built from `firmware/ethology/` (14 files), **plus `CogDisplay` if you pick the *Giga + Display* board**. |
| **Robot firmware** | `arduino/ethology_ble_robot/` | Your BLE robot: receives a hierarchy over BLE, heartbeat LED, `CogDisplay`. Open this folder directly in the IDE. |

`CogBluetooth` and `CogDisplay` are deliberately absent from the student
download: bundling them would make every student sketch require ArduinoBLE and
Arduino_GigaDisplay_GFX, neither of which a bare sketch has any use for.

**Stage 1 uses the student sketch. Stages 2–4 use `arduino/ethology_ble_robot/`.
Stage 5 uses both.**

---

## Stage 0 — Web app (laptop only, ~20 min)

- [ ] `python3 -m http.server 8000`, open `http://localhost:8000`.
      **Not** by double-clicking `index.html` — `fetch` has no origin on
      `file://` and the firmware won't load.
- [ ] Build a hierarchy; confirm the preview updates as you reorder.
- [ ] Toggle both sketch styles and all four themes; confirm both persist.
- [ ] Move cruise off the bottom; confirm the warning and the unrolled
      view's markers.
- [ ] Download the sketch folder. Unzip. 15 files, `.ino` basename matching
      its folder.
- [ ] Switch the board to **Giga + Display** and download again. 17 files now —
      `CogDisplay.h/.cpp` are added and the sketch gains `display.` calls.

**Pages note:** private repos need GitHub Pro, Team or Enterprise — Free
publishes public repos only. Local serving covers every stage below, so this
only blocks student access.

---

## Stage 1 — Student sketch compiles (Giga, no robot, ~30 min)

Uses the folder you downloaded and unzipped in Stage 0.

- [ ] Install the Arduino GIGA core, **4.0.6 or later**.
- [ ] Open the downloaded sketch, board *Arduino GIGA R1 WiFi*, Verify.
- [ ] Repeat for the unrolled style and for a cruise-in-the-middle hierarchy
      (it must compile — the dead rungs are meant to be observable at runtime).
- [ ] Install **Arduino_GigaDisplay_GFX**, then compile the *Giga + Display*
      download. With the shield attached it should show `RUNNING`. That alone
      is the student-facing win: a robot that boots and says nothing is
      indistinguishable from a dead battery.
- [ ] Set `PAW_DISPLAY_DEV` to 1 in the downloaded `CogDisplay.h` and
      recompile. Rung list, guard dots and sensor strip appear — without any
      BLE link or robot firmware involved. **This is your bench setup.**

**Proves:** emitted code and bundled firmware agree, and `Servo.h` builds for
`mbed_giga`.

---

## Stage 2 — Robot firmware, classroom build (Giga, no robot, ~20 min)

Different sketch: open `arduino/ethology_ble_robot/ethology_ble_robot.ino`.
Everything below lives there, not in the downloaded student sketch.

- [ ] Install **ArduinoBLE**.
- [ ] Verify with defaults (`PAW_USE_DISPLAY 0`). Must compile with no display
      library installed at all — that is the classroom path.
- [ ] Upload. The heartbeat is the on-board LED driven by `heartbeat()` in the
      `.ino`: blinking every 500 ms while advertising or listening, solid once
      a hierarchy is running. On Giga the LED is active-LOW, handled by
      `PAW_LED_ACTIVE_LOW`; if it reads inverted (dark = running), that define
      is wrong for your board.
- [ ] Open Serial Monitor at 9600. Confirm the robot advertises as
      `Robot<n>`.

---

## Stage 3 — Display (Giga + Display Shield, no robot, ~1 hr)

Same sketch as Stage 2. `CogDisplay.h/.cpp` are already in that folder; they
are not part of the student download and never will be.

- [ ] Install **Arduino_GigaDisplay_GFX**.
- [ ] Set `PAW_USE_DISPLAY` to 1. Easiest is to edit the `#define` near the top
      of the `.ino` — the IDE has no field for `-D` flags without a
      `build_opt.h`. Same for `PAW_DISPLAY_DEV` in `CogDisplay.h`. Expect BLE status and session only —
      this is the classroom build, and it must not show sensors or rungs.
- [ ] Confirm the image is landscape and right way up. `begin()` sets
      `DISPLAY_ROTATION = 1`; use 3 if it appears upside down.
- [ ] Set `PAW_DISPLAY_DEV` to 1 in `CogDisplay.h`. Send a hierarchy. Expect the rung list, guard
      dots and sensor strip.
- [ ] **With nothing wired**, read the guard dots. Note which float true —
      that is your baseline for "no sensors attached" and tells you which pins
      need hardware before which behaviours mean anything.
- [ ] Ground a bumper pin by hand. Confirm its dot changes and the highlight
      moves. That one test validates the whole guard-vs-fired display.

---

## Stage 4 — Hardware, one subsystem at a time

Add one thing, confirm it on the display, add the next. The sensor strip
separates a wiring fault from a logic fault.

- [ ] **Drive** — servos on D6/D5, cruise-only hierarchy.
- [ ] **Front bumpers** — D2 right, D4 left. `escape_front` above cruise.
- [ ] **Proximity** — A1 right, A0 left. `avoid_object` above cruise.
- [x] **`PROX_THRESHOLD` = 35 confirmed good on hardware.** Note the mapping
      still saturates at 18 cm, so a wall at 18 cm and one at 2 cm read the
      same — relevant if you ever raise the threshold.
- [ ] **Light** — A3 right, A2 left. `approach_light` above cruise. Sweep a
      lamp; watch LIGHT GRAD cross ±15.
- [ ] **Back bumpers — D7 right, D8 left.** Left moved off D3, which is PWM.
      The prototype has no back bumpers, so treat `escape_back` as untested
      until they are physically wired.
- [ ] **Cruise Arc** — with a cruise-arc-only hierarchy, confirm visible
      curves that change direction roughly every 1–2 s. If it tracks straight,
      `ARC_HOLD_MIN_MS`/`MAX_MS` are too close to `CRUISE_SECONDS`.
- [ ] **Light polarity** — sweep a lamp past `approach_light` and confirm the
      robot turns toward it. The signs were reversed once; they are now set
      from hardware, not derivation.

---

## Stage 5 — End to end

- [ ] Build a four-behaviour hierarchy on the web app, download, compile,
      upload. **This is the student sketch** — no display, no BLE. Behaviour
      is observed from the robot itself.
- [ ] Then send the same hierarchy to the robot firmware over BLE and compare.
      Same rungs, same order, and now visible on the display.
- [ ] Drive into an obstacle; confirm the highlight jumps to the escape rung
      and returns to cruise.
- [ ] Build a deliberately broken hierarchy (cruise in the middle). Confirm it
      compiles and the rungs below never highlight while showing lit dots and
      `SUBSUMED`. That is the debrief artifact.
- [ ] Rebuild without `PAW_DISPLAY_DEV`. Confirm identical behaviour.

---

## Still open on the Python side

Not blocking any stage above; tracked in `PORTING-NOTES.md`. The one with
teeth is the legacy wire names: `EthologyRobot.h` states the Python side still
sends `seek_light` and `escape_rear`, and `setHierarchy()` rejects a hierarchy
**whole** on an unknown name — so any live hierarchy containing them is
silently unusable.
