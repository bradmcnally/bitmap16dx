#include "platform/power.h"

#include <M5Cardputer.h>

uint8_t Power::getBatteryPercent() {
  const int percent = M5Cardputer.Power.getBatteryLevel();
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return static_cast<uint8_t>(percent);
}
