#include "platform/clock.h"

#include <Arduino.h>

uint32_t Clock::nowMs() {
  return millis();
}
