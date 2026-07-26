#pragma once

#include <cstdint>

#include "core/canvas.h"
#include "core/settings.h"

namespace bitmap16 {
namespace SettingsView {

enum class Action : uint8_t {
  None,
  ThemeChanged,
  DefaultGridChanged,
  MatrixUnitsChanged,
  MatrixRotationChanged,
  ExportFormatChanged,
  ShakeUndoChanged,
  BluetoothRequested,
};

struct State {
  int cursor = 0;
  int scrollOffset = 0;
};

struct Theme {
  uint16_t background = 0;
  uint16_t text = 0xffff;
  uint16_t textSecondary = 0x7bef;
};

int itemCount(
    bool includeBluetooth, bool includeMatrix, bool includeShakeUndo);
bool moveCursor(
    State& state,
    int delta,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo);
Action activate(
    State& state,
    Settings& settings,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo);
void render(
    Canvas& canvas,
    State& state,
    const Settings& settings,
    const Theme& theme,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    const char* bluetoothValue = nullptr,
    const char* statusMessage = nullptr);

}  // namespace SettingsView
}  // namespace bitmap16
