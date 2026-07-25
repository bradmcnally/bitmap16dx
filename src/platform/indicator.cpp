#include "platform/indicator.h"

#include <Arduino.h>

namespace {

constexpr uint8_t kRgbPin = 21;
constexpr uint8_t kEnablePin = 38;

}  // namespace

void Indicator::init() {
  pinMode(kEnablePin, OUTPUT);
  digitalWrite(kEnablePin, HIGH);
  off();
}

void Indicator::setColor(uint8_t red, uint8_t green, uint8_t blue) {
  rgbLedWrite(kRgbPin, red, green, blue);
}

void Indicator::off() {
  setColor(0, 0, 0);
}
