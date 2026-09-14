// Stands in for the .ino: a separate translation unit from CogDisplay.cpp,
// which is exactly the arrangement that produced the undefined references.
#include "PAWConfig.h"
#include "CogDisplay.h"
#include "EthologyRobot.h"
EthologyRobot bot;
CogDisplay display;
static const char* const H[] = {"escape_front","cruise_straight"};
int main() {
  display.begin();
  display.setHierarchy(H, 2);
  bool g[CogDisplay::MAX_ROWS];
  for (int i=0;i<bot.hierarchyLength();i++) g[i]=bot.guardMet(bot.behaviorAt(i));
  display.setGuards(g, bot.hierarchyLength());
  display.setActive(bot.lastFiredIndex());
  EthologyRobot::SensorSnapshot s = bot.snapshot();
  display.setSensors(s.leftProx,s.rightProx,s.lightGradient,
                     s.leftFrontBump,s.rightFrontBump,s.leftBackBump,s.rightBackBump);
  display.update(); display.updateStatus(); display.toggleVerbose();
  return 0;
}
