# Hierarchy Builder — web edition

A browser port of the RE Hierarchy Builder standalone. Students arrange
behaviors into a subsumption hierarchy and download the matching Arduino
sketch. No install, no Python, no PyGame.

Two things: `index.html` and the `firmware/ethology/` sources it packages with
each download. No build step, no dependencies.

## Sending over Bluetooth

Pick a robot (A–D) in the header, build a hierarchy, press **Send over
Bluetooth**. The robot must be running the receiver sketch and advertising.

The same robot letter does two jobs, deliberately: it stamps `PAW_ROBOT_ID`
into a downloaded receiver sketch, and it filters the scan when sending. The
firmware's own comment records why that matters — every board flashed from an
unedited copy came up as `RobotA`, so `RobotB` was unreachable and two powered
robots collided.

**Send waits for the robot's answer, not just a successful write.** Writing
`{"cmd":"run"}` only stages the names; the robot validates them against
`EthologyRobot`'s vocabulary and replies on the DATA characteristic with
`running` or an error. A rejected name surfaces here rather than leaving a
robot that quietly does nothing.

A run is a one-way door by firmware design, so a second send to a running
robot returns `busy`. Power-cycle to send another.

Chromium only (no Safari, no Firefox), and https or localhost. The download
path remains the universal fallback.

## BLE receiver sketch

**BLE receiver sketch** downloads scaffolding with *no hierarchy baked in* —
`EthologyRobot`, `CogBluetooth`, `CogDisplay` and the sketch that wires them
together. Flash it once per robot; after that hierarchies arrive over the air
and nobody touches the Arduino IDE again.

19 files. The only per-robot difference is `PAW_ROBOT_ID`, set from the picker.

## The display setting

**Off** — no display code at all. Compiles without `Arduino_GigaDisplay_GFX`.

**Status** — `RUNNING`, `ADVERTISING`, or `BAD HIERARCHY` if `setHierarchy()`
rejected a name. The classroom setting: students still infer the hierarchy
from behaviour.

**Full HUD** — adds the rung list with per-rung guard dots and a raw sensor
strip.

The setting is written into **`PAWConfig.h`**, which ships with every download
and holds the robot letter, display mode and trace flag. Edit it there if you
want to change your mind after downloading.

It has to be a header, not a `#define` in the `.ino`: `CogDisplay.cpp` compiles
separately and only sees values from a header it also includes. A define in the
sketch reaches the sketch alone and the link fails with `undefined reference to
CogDisplay::setGuards`.

You can also type `v` in the Serial Monitor to toggle the HUD live without
recompiling, in a build where `PAW_DISPLAY_DEV` is 1.

The status tier matters more than it looks: a robot that silently does nothing
because one behaviour name was rejected is indistinguishable from a dead
battery, and that is the most common way to lose a lab period.

**Unrolled style + display board shows status only.** That style writes the
rungs out rather than installing them, so `bot.hierarchy()` is never called and
`lastFiredIndex()` would always be -1. The app warns when the two are paired.

## Keeping the firmware current

`firmware/ethology/` is a copy. The main PAW-Robotics tree's
`firmware/shared/` is the authority. After any change there:

```bash
./sync-firmware.sh ~/path/to/PAW-Robotics
```

It copies the dependency closure of `EthologyRobot.h`, then cross-checks that
`FIRMWARE_FILES` in `index.html` matches what is on disk — a file present but
unlisted is silently omitted from every download.

`firmware/hud/` moves the other way: `CogHUD` is new here and belongs in the
main project's `firmware/shared/`.

## Deploy to GitHub Pages

1. Commit `index.html` to a repository.
2. Settings → Pages → Source: *Deploy from a branch*, branch `main`, folder `/ (root)`.
3. The site appears at `https://<user>.github.io/<repo>/`.

If you put it in a subfolder, everything still works — there are no absolute
paths. Add an empty `.nojekyll` file at the repo root only if you later add
directories beginning with an underscore.

## What it does

- Two-column pool and hierarchy, with arrow buttons to move and reorder.
- Live sketch preview that regenerates on every change.
- **Display setting** in the header: **Off / Status / Full HUD**. It applies to
  every download — the generated hierarchy sketch *and* the BLE receiver — by
  stamping `PAW_USE_DISPLAY` and `PAW_DISPLAY_DEV` into each sketch. When it is
  not Off, `CogDisplay.h/.cpp` are added to the ZIP.
- **Two sketch styles.** *Hierarchy object* (default) declares the ordering as
  data and calls `bot.setHierarchy()` / `bot.hierarchy()`, letting the robot
  arbitrate — the same path a BLE-delivered hierarchy takes. *Unrolled
  if/else* writes every rung out for reading in a debrief. Toggle sits in the
  preview header; the choice persists.
- **Download sketch folder** — a ZIP holding the `.ino` plus every firmware
  source it includes. Unzip, open the `.ino`, compile. Nothing to install.
- **Sketch only** — the bare `.ino`, for dropping into a folder that already
  has the sources.
- Copy to clipboard.
- Four themes, ported from `engine/theme.py`. The choice persists in
  `localStorage`.

Keyboard: `→` and `←` move a selected behavior between columns, `Alt+↑` and
`Alt+↓` reorder, `Enter` on a focused item moves it.

## Differences from `codegen.py`

Two deliberate changes. Both should be ported back to the Python version.

**1. The generated `hierarchy()` calls `bot.readSensors()` first.**

The Python original omits it. The generated function is a free function in the
sketch, not `EthologyRobot::hierarchy()`, so nothing refreshes the cached
sensor members that every guard reads — `_leftProxData`, `_lightGradient`,
`_leftFrontBumpData` and the rest. Those members have no initializer, so they
hold indeterminate values that never change. `collisionThreshold()` returns
true when `_leftFrontBumpData == 0`, so a hierarchy containing `escape_front`
can latch into escaping forever.

**2. Cruise only becomes a bare `else` when it is last.**

Anywhere else it emits `if (true)` or `else if (true)`. The Python version
emits a bare `else` at any position, which produces `else` followed by
`else if` — a syntax error, so the sketch never compiles.

With `if (true)`, a misplaced cruise compiles and uploads, and the rungs below
it are visibly unreachable. The preview flags them in the margin. That is the
intended lab-debrief artifact: students watch the robot ignore everything
below cruise, then read why in the code.

Generated sketches were checked with `g++ -fsyntax-only` against stub headers
for all five shapes: valid, cruise-first, cruise-middle, cruise-only, and
no-cruise.

## How the sketch folder is assembled

A bare `.ino` will not compile. It includes `EthologyRobot.h` and
`CogServo.h`, which are project files rather than installed libraries. The
Arduino IDE compiles every `.cpp` and `.h` beside the `.ino`, so the download
is a folder:

```
hierarchy_20260826_143022/
  hierarchy_20260826_143022.ino
  EthologyRobot.h / .cpp    Robot.h / .cpp
  CogServo.h / .cpp         CogAnaDigi.h / .cpp
  CogProximity.h / .cpp     CogLight.h / .cpp
  CogCollision.h / .cpp
```

Arduino requires the `.ino` basename to match its folder, so both use the same
timestamped stem.

The sources are fetched from this site's own `firmware/ethology/` directory at
download time rather than embedded in the HTML, so the repository copy stays
authoritative — update the firmware and the next download picks it up with no
edit to `index.html`. Keep `FIRMWARE_FILES` in sync if you add a class.

That file set is the dependency closure of `EthologyRobot.h`: your
`firmware/ethology_standalone/` contents, which is `shared/` minus
`CogPotentialField` and `CogVisLight`.

The ZIP is written by a small store-only (uncompressed) writer in
`index.html`, so there is no library to load. Output was verified with
`unzip -t`, and extracted sources are byte-identical to the repo copies.

**One caveat:** opening `index.html` by double-clicking it uses the `file://`
protocol, where `fetch` has no origin and the firmware can't be read. The
hosted page is fine. Locally, run `python3 -m http.server` in the folder and
open `http://localhost:8000`. The status line says so if it happens.

## Hardware profile

`HARDWARE_PROFILES` in `index.html` mirrors `robots/hardware_profiles.json`.
The default is now `giga_r1_wifi__prototype`; the R4 entry is kept for
reference. Motor pins live in `ROBOT_CONFIG` (D6 / D5) — the only part of
`robot.json` that code generation actually consumes.

`sketch_includes` is empty for both profiles. `CogServo.h` includes
`<Servo.h>` itself, so listing it only duplicated the include in every
generated sketch.

**Servo.h on Giga:** `EthologyRobot.h` claims `Servo.h` does not list
`mbed_giga` among its supported architectures and advises using the PCA9685
constructor instead. That constructor does not exist — `CogServo.h` records
that the PCA9685 path was removed. The comment also appears to be wrong in
practice: `Servo.h` works on the Giga with `writeMicroseconds()`, which is
what `CogServo` uses. The comment is worth deleting so it stops sending people
after a constructor that isn't there.

## Not included

Transports. The desktop app's Launch Arduino and Send via BLE have no browser
equivalent as written. When they come, they go behind a common adapter
interface alongside the download:

- `DownloadIno` — works in every browser (this is what ships now)
- `UsbSerial` — Web Serial, works with every board over USB
- `BleGatt` — Web Bluetooth, matching the WinRT GATT path the desktop app uses

Web Serial and Web Bluetooth are Chromium-only and require HTTPS, which
GitHub Pages provides.

## Legacy wire names

`canonicalBehavior()` is carried over from `codegen.py` but is currently
unreachable — nothing loads a saved hierarchy yet. It is here so that a future
load/save feature has it. If the aliases are retired in the Python version,
delete `LEGACY_BEHAVIOR_ALIASES` here too.
