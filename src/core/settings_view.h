#pragma once

#include <cstdint>

#include "core/canvas.h"
#include "core/settings.h"

namespace bitmap16 {
namespace SettingsView {

enum class Page : uint8_t {
  Main,
  RgbMatrix,
  IndicatorLed,
};

enum class Action : uint8_t {
  None,
  ThemeChanged,
  DefaultGridChanged,
  MatrixMenuRequested,
  MatrixEnabledChanged,
  MatrixUnitsChanged,
  MatrixRotationChanged,
  MatrixBrightnessChanged,
  IndicatorMenuRequested,
  IndicatorPaletteChanged,
  IndicatorLowBatteryChanged,
  ExportFormatChanged,
  ShakeUndoChanged,
  SaveWarningsChanged,
  BluetoothRequested,
  QuitRequested,
};

struct State {
  int cursor = 0;
  int scrollOffset = 0;
  Page page = Page::Main;
};

struct Theme {
  uint16_t background = 0;
  uint16_t text = 0xffff;
  uint16_t textSecondary = 0x7bef;
};

int itemCount(
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit = false,
    bool includeIndicator = false);
bool moveCursor(
    State& state,
    int delta,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit = false,
    bool includeIndicator = false);
Action activate(
    State& state,
    Settings& settings,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit = false,
    bool includeIndicator = false);
void render(
    Canvas& canvas,
    State& state,
    const Settings& settings,
    const Theme& theme,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    const char* bluetoothValue = nullptr,
    const char* statusMessage = nullptr,
    bool includeQuit = false,
    bool includeIndicator = false);

}  // namespace SettingsView
}  // namespace bitmap16
