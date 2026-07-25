#include "platform/led_matrix.h"

#include <FastLED.h>
#include <new>

#include "core/led_mapping.h"

namespace {

constexpr uint8_t kDataPin = 2;
constexpr uint16_t kMaxLedCount = 256;

CRGB* leds = nullptr;
uint8_t configuredUnits = 1;
uint8_t configuredRotation = 2;
bool enabled = false;

uint8_t gridSize() {
  return configuredUnits == 4 ? 16 : 8;
}

}  // namespace

bool LEDMatrix::init() {
  if (leds != nullptr) {
    return true;
  }
  leds = new (std::nothrow) CRGB[kMaxLedCount];
  if (leds == nullptr) {
    return false;
  }
  FastLED.addLeds<WS2812, kDataPin, GRB>(leds, kMaxLedCount);
  FastLED.clear();
  FastLED.show();
  return true;
}

void LEDMatrix::setConfiguration(uint8_t matrixUnits, uint8_t rotation) {
  configuredUnits = matrixUnits == 4 ? 4 : 1;
  configuredRotation = rotation % 4;
}

void LEDMatrix::setEnabled(bool shouldEnable) {
  enabled = shouldEnable;
  if (!enabled && leds != nullptr) {
    FastLED.clear();
    FastLED.show();
  }
}

bool LEDMatrix::isEnabled() {
  return enabled;
}

void LEDMatrix::setBrightness(uint8_t percent) {
  if (percent > 100) {
    percent = 100;
  }
  FastLED.setBrightness(
      static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255) / 100));
}

void LEDMatrix::clear() {
  if (leds != nullptr) {
    FastLED.clear();
  }
}

bool LEDMatrix::setPixelRgb565(uint8_t x, uint8_t y, uint16_t color) {
  const bitmap16::LedMapping::Rgb888 rgb =
      bitmap16::LedMapping::rgb565ToRgb888(color);
  return setPixelRgb888(x, y, rgb.red, rgb.green, rgb.blue);
}

bool LEDMatrix::setPixelRgb888(
    uint8_t x,
    uint8_t y,
    uint8_t red,
    uint8_t green,
    uint8_t blue) {
  const uint8_t size = gridSize();
  if (leds == nullptr || !enabled || x >= size || y >= size) {
    return false;
  }

  const uint16_t index =
      bitmap16::LedMapping::indexFor(
          x,
          y,
          configuredUnits,
          configuredRotation);
  leds[index] = CRGB(red, green, blue);
  return true;
}

void LEDMatrix::show() {
  if (leds != nullptr && enabled) {
    FastLED.show();
  }
}
