#pragma once

#include <cstdint>

#include "core/canvas.h"

namespace bitmap16 {
namespace HelpView {

struct State {
  int cursor = 0;
  int scrollOffset = 0;
};

struct Theme {
  uint16_t background = 0;
  uint16_t text = 0xffff;
  uint16_t textSecondary = 0x7bef;
};

int itemCount(bool includeLedMatrixControls, bool includeBatteryControls);
bool moveCursor(
    State& state,
    int delta,
    bool includeLedMatrixControls,
    bool includeBatteryControls);
void render(
    Canvas& canvas,
    State& state,
    const Theme& theme,
    bool includeLedMatrixControls,
    bool includeBatteryControls);

}  // namespace HelpView
}  // namespace bitmap16
