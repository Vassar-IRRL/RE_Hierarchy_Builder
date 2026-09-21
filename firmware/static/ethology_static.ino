/*
  ethology_static.ino
  -------------------
  A fixed hierarchy, written out rung by rung. No BLE, no hierarchy
  installation, nothing to configure but the display.

      escapeFrontCollision()  ->  avoidObject()  ->  approachLight()
                              ->  cruiseStraight()

  WHY THIS EXISTS
  ---------------
  Two jobs the other sketches cannot do.

  As a REFERENCE: it depends on nothing but the robot itself. If a robot
  misbehaves under a downloaded or BLE-delivered hierarchy, flashing this
  separates "the behaviours are wrong" from "the delivery is wrong". There is
  no radio, no session, no name matching, no parsing — if this misbehaves, the
  fault is in EthologyRobot or the wiring.

  As a TEACHING ARTIFACT: the arbitration is visible. EthologyRobot::hierarchy()
  does exactly what loop() does below, but does it inside a loop over a table,
  which is harder to read than four if/else rungs. Students can see that
  priority IS the order of the branches, and that a behaviour only runs when
  every behaviour above it declined.

  WHAT IT DOES NOT DO
  -------------------
  No hierarchy is installed in EthologyRobot — the rungs are written here
  instead. So bot.lastFiredIndex() stays -1 and bot.guardMet() would describe
  rungs the robot is not arbitrating. The display therefore shows status only,
  whatever PAW_DISPLAY_DEV says. Use the BLE firmware if you want the HUD.

  READ THE SENSORS FIRST
  ----------------------
  The guards below do not sample anything. They compare values cached by
  readSensors(), which also halts the motors so that every guard and every
  behaviour in one tick sees the same world. EthologyRobot::hierarchy() calls
  it for you; this sketch hand-writes its rungs, so it must call it itself.
  Without that line the guards read stale data forever and the robot latches
  into whatever the first tick happened to decide.

  Files required in this sketch folder:
      ethology_static.ino     (this file)
      PAWConfig.h             (build switches)
      CogDisplay.h / .cpp     (GIGA Display Shield; compiles out when off)
      EthologyRobot.h / .cpp  (behaviours)
      Robot.h / .cpp          (drivetrain base)
      CogServo.h / .cpp       (servo driver)
      CogProximity.h / .cpp   (IR sensors)
      CogLight.h / .cpp       (LDR sensors)
      CogCollision.h / .cpp   (bumpers)
      CogAnaDigi.h / .cpp     (sensor base)

  Hardware
  --------
      D6   Left servo              A0   Left proximity
      D5   Right servo             A1   Right proximity
      D4   Left front bumper       A2   Left light
      D2   Right front bumper      A3   Right light
      D8   Left back bumper        D7   Right back bumper

  The back bumpers are not used by this hierarchy.
*/

#include "PAWConfig.h"

#include "CogServo.h"
#include "EthologyRobot.h"
#include "CogDisplay.h"

constexpr uint8_t LEFT_SERVO_PIN  = 6;
constexpr uint8_t RIGHT_SERVO_PIN = 5;

EthologyRobot bot;
CogDisplay    display;

void setup()
{
    Serial.begin(9600);

    // The LED first: it is the only sign of life that survives losing USB and
    // the display. Solid ON here means running, because this sketch has no
    // waiting state to distinguish — it runs from power-on.
    pinMode(LED_BUILTIN, OUTPUT);
    LED_WRITE(true);

    bot.begin(LEFT_SERVO_PIN, RIGHT_SERVO_PIN);

    display.begin();
    display.setStatus("RUNNING", "static hierarchy");
}

void loop()
{
    // Sense once, then decide. See the note at the top of this file.
    bot.readSensors();

    // Highest priority first. The first rung whose condition is met wins the
    // tick; everything below it is subsumed. Cruise has no condition, so it
    // always fires and nothing can be placed after it.

    if (bot.collisionThreshold()) {          // 1. Escape Front
        bot.escapeFrontCollision();
    }
    else if (bot.proximityThreshold()) {     // 2. Avoid Object
        bot.avoidObject();
    }
    else if (bot.lightGradientThreshold()) { // 3. Approach Light
        bot.approachLight();
    }
    else {                                   // 4. Cruise Straight
        bot.cruiseStraight();
    }

    display.updateStatus();
}
