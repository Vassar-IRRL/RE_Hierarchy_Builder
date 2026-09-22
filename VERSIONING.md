# Versioning

The Hierarchy Builder uses **Semantic Versioning** (semver.org):
`MAJOR.MINOR.PATCH`, shown in the header as `v1.0.0`.

The number lives in one place — `APP_VERSION` in `index.html` — and is carried
from there into the page header, the footer, the header comment of every
generated sketch, and `PAW_BUILDER_VERSION` in every downloaded `PAWConfig.h`.
The BLE receiver prints it on the serial port at boot. So any sketch on any
robot can be traced back to the release that produced it.

What changed in each release goes in `CHANGELOG.md`.

---

## The one question

SemVer defines MAJOR as a break in compatibility. For this project,
compatibility means:

> **Does anything already flashed, downloaded, or installed stop working, or
> need to be redone?**

Yes → MAJOR. No, but something new is possible → MINOR. No, and nothing new →
PATCH.

The things that can "already exist" out in the world are: firmware flashed to
robots, sketch folders students have downloaded, libraries installed in the
Arduino IDE, wiring on the robots, and the habits of people using the page.

---

## MAJOR — `v2.0.0`

Something already out there must be redone.

- **The BLE protocol changes.** New service or characteristic UUIDs, a renamed
  command, a different JSON shape. Every flashed receiver has to be reflashed
  before Send over BLE works again.
- **A behavior wire name is renamed or removed.** `setHierarchy()` rejects a
  hierarchy *whole* on an unknown name, so this silently breaks robots.
- **A `PAWConfig.h` setting is renamed, removed, or changes meaning.**
  Downloaded folders and edited configs stop compiling or mean something else.
- **A new library becomes required**, or the minimum GIGA core version rises.
  Every machine needs setup it did not need before.
- **Pin assignments change.** Robots must be rewired.
- **A workflow disappears or changes shape** — removing a button people rely
  on, or changing what one produces in a way that breaks how it is used.

**The Mac example.** If Send over BLE on macOS turns out to need only a
clearer message for unsupported browsers, that is a MINOR or PATCH release.
If it turns out to need changes to the receiver firmware or the BLE protocol,
then every robot must be reflashed — MAJOR.

When MAJOR goes up, MINOR and PATCH reset: `v1.4.2` → `v2.0.0`.

## MINOR — `v1.1.0`

Something new is possible; nothing existing breaks.

- A new setting or picker (the light-sensor type was one)
- A new behavior, added so old hierarchies still work
- A new board profile or display mode
- A new *optional* library, behind a switch that defaults to off
- A new workflow alongside the existing ones

When MINOR goes up, PATCH resets: `v1.3.4` → `v1.4.0`.

## PATCH — `v1.0.1`

Nothing new, nothing broken. Fixes and adjustments.

- Bug fixes, including a behavior that did the wrong thing
- Tuning constants: speeds, thresholds, durations
- Tooltip and message wording, layout, styling
- Documentation and tooling

---

## Judgment calls

**Tuning that changes what the robot visibly does** — cruise speed, escape
arcs — is still PATCH. It changes behavior, but nothing already flashed or
downloaded stops working, and the robot is still doing the same *job*.
Record it in the changelog so it is not a surprise.

**A fix that forces a reflash** is MAJOR even though it is a fix. The question
is what the user has to redo, not how the change feels to write.

**When unsure between two levels, take the higher one.** A version number that
overstates a change costs nothing; one that understates it sends someone to
debug a robot that was never going to work.

---

## Releasing

1. Change `APP_VERSION` in `index.html`.
2. Add a section to the top of `CHANGELOG.md` with the date.
3. Commit, merge, and tag the merge on `main`:

   ```
   git checkout main
   git pull
   git tag v1.1.0
   git push origin v1.1.0
   ```

A tag marks the exact commit a version came from, so a robot reporting
`v1.1.0` on its serial port can be matched to its source at any time.

`BUILD_ID` in the footer is separate. It changes on every edit to `index.html`
and exists to catch stale browser caches; it is not a release number.
