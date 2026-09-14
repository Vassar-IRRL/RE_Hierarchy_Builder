#ifndef PAWCONFIG_H
#define PAWCONFIG_H

// ═════════════════════════════════════════════════════════════════════════════
// Build configuration. EDIT THIS FILE — everything else reads it.
//
// WHY A SEPARATE FILE. These settings change which methods CogDisplay declares,
// so every translation unit has to agree on them. A #define in the .ino only
// applies to the .ino: CogDisplay.cpp is compiled separately and would not see
// it, so the sketch would declare real methods while the library compiled
// empty inline ones, and the link would fail with
//
//     undefined reference to `CogDisplay::setGuards(bool const*, int)'
//
// Putting them here means the .ino and CogDisplay.cpp read the same values.
// Setting them in the .ino does not work, however obvious it looks.
//
// Sketches downloaded from the Hierarchy Builder arrive with this file already
// filled in from the settings you chose there.
// ═════════════════════════════════════════════════════════════════════════════


// ── Robot identity ───────────────────────────────────────────────────────────
// Which robot this board IS. The advertising name becomes "Robot" + this
// letter, and that is what the Builder scans for when it sends a hierarchy.
//
// Every board flashed from an unedited copy comes up as RobotA, so two powered
// robots collide and RobotB is unreachable. Change the letter per board.
#ifndef PAW_ROBOT_ID
#define PAW_ROBOT_ID A
#endif


// ── Display ──────────────────────────────────────────────────────────────────
// 0 = no display code at all. Compiles on a board with no shield attached and
//     without the Arduino_GigaDisplay_GFX library installed.
// 1 = GIGA Display Shield. Needs Arduino_GigaDisplay_GFX and GIGA core 4.0.6+.
#ifndef PAW_USE_DISPLAY
#define PAW_USE_DISPLAY 0
#endif

// How much the display shows. Ignored unless PAW_USE_DISPLAY is 1.
//
// 0 = status only: RUNNING, ADVERTISING, BAD HIERARCHY. The classroom setting.
//     Students still have to infer the hierarchy from watching the robot.
// 1 = full diagnostic: the rung list, a lit dot on every rung whose condition
//     is currently met, and the raw sensor readings. With this on you can also
//     type 'v' in the Serial Monitor to toggle the HUD live.
#ifndef PAW_DISPLAY_DEV
#define PAW_DISPLAY_DEV 0
#endif


// ── Bench tracing ────────────────────────────────────────────────────────────
// One sensor line per tick on the serial port, rate-limited. Independent of
// the display; useful when there is no shield attached.
#ifndef PAW_SENSOR_TRACE
#define PAW_SENSOR_TRACE 0
#endif


// ── Derived ──────────────────────────────────────────────────────────────────
// Not settings. Do not edit.
#define _PAW_STR2(x) #x
#define _PAW_STR(x)  _PAW_STR2(x)
#define PAW_ROBOT_NAME ("Robot" _PAW_STR(PAW_ROBOT_ID))

#endif // PAWCONFIG_H
