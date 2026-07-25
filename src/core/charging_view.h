#pragma once

#include <cstdint>

#include "core/canvas.h"

namespace bitmap16 {
namespace ChargingView {

constexpr int kItemCount = 5;
constexpr int kBatteryItem = 3;
constexpr int kSketchItem = 4;

struct Item {
  float x = 0.0f;
  float y = 0.0f;
  float dx = 0.0f;
  float dy = 0.0f;
  const uint8_t* icon = nullptr;
};

struct State {
  Item items[kItemCount] = {};
  int batteryPercent = 0;
  bool sketchAvailable = false;
};

struct Theme {
  uint16_t background;
  uint16_t iconDark;
  uint16_t iconLight;
  uint16_t text;
};

struct SketchImage {
  const uint8_t (*pixels)[16] = nullptr;
  uint8_t gridSize = 8;
  const uint16_t* paletteColors = nullptr;
  uint8_t paletteSize = 16;
};

void initialize(
    State& state,
    int width,
    int height,
    uint32_t seed,
    const uint8_t* const icons[4],
    int batteryPercent,
    bool sketchAvailable);
void setBattery(State& state, int batteryPercent, const uint8_t* icon);
void update(State& state, int width, int height);
void render(
    Canvas& canvas,
    const State& state,
    const Theme& theme,
    const SketchImage* sketch = nullptr);

}  // namespace ChargingView
}  // namespace bitmap16
