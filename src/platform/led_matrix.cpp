#include "platform/led_matrix.h"

#include <FastLED.h>

#include "core/led_mapping.h"

namespace {

constexpr uint8_t kDataPin = 2;
constexpr uint16_t kMaxLedCount = 256;

CRGB sourceLeds[kMaxLedCount];
CRGB outputLeds[kMaxLedCount];
bool initialized = false;
uint8_t configuredUnits = 1;
uint8_t configuredRotation = 2;
uint8_t brightnessPercent = 100;
bool enabled = false;

uint8_t gridSize() {
  return configuredUnits == 4 ? 16 : 8;
}

void transmitAndWait() {
  FastLED.show();
  // The IDF5 RMT backend is asynchronous. A 256-pixel WS2812 frame takes
  // about 7.7 ms, so keep the source/output buffers and adjacent peripherals
  // untouched until the transmission has completed.
  delay(9);
}

}  // namespace

bool LEDMatrix::init() {
  if (initialized) {
    return true;
  }
  FastLED.addLeds<WS2812, kDataPin, GRB>(outputLeds, kMaxLedCount);
  FastLED.setBrightness(255);
  FastLED.setDither(DISABLE_DITHER);
  FastLED.clear();
  transmitAndWait();
  initialized = true;
  return true;
}

void LEDMatrix::setConfiguration(uint8_t matrixUnits, uint8_t rotation) {
  configuredUnits = matrixUnits == 4 ? 4 : 1;
  configuredRotation = rotation % 4;
}

void LEDMatrix::setEnabled(bool shouldEnable) {
  enabled = shouldEnable;
  if (!enabled && initialized) {
    fill_solid(sourceLeds, kMaxLedCount, CRGB::Black);
    FastLED.clear();
    transmitAndWait();
  }
}

bool LEDMatrix::isEnabled() {
  return enabled;
}

void LEDMatrix::setBrightness(uint8_t percent) {
  brightnessPercent = percent > 100 ? 100 : percent;
}

void LEDMatrix::clear() {
  if (initialized) {
    fill_solid(sourceLeds, kMaxLedCount, CRGB::Black);
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
  if (!initialized || !enabled || x >= size || y >= size) {
    return false;
  }

  const uint16_t index =
      bitmap16::LedMapping::indexFor(
          x,
          y,
      configuredUnits,
      configuredRotation);
  sourceLeds[index] = CRGB(red, green, blue);
  return true;
}

void LEDMatrix::show() {
  if (initialized && enabled) {
    for (uint16_t index = 0; index < kMaxLedCount; ++index) {
      outputLeds[index] = CRGB(
          static_cast<uint8_t>(
              (static_cast<uint16_t>(sourceLeds[index].r) *
                   brightnessPercent +
               50) /
              100),
          static_cast<uint8_t>(
              (static_cast<uint16_t>(sourceLeds[index].g) *
                   brightnessPercent +
               50) /
              100),
          static_cast<uint8_t>(
              (static_cast<uint16_t>(sourceLeds[index].b) *
                   brightnessPercent +
               50) /
              100));
    }
    transmitAndWait();
  }
}
