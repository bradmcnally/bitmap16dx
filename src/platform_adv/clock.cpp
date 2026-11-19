#include "platform_adv/clock.h"

#include <Arduino.h>

uint32_t Clock::nowMs() {
  return millis();
}
