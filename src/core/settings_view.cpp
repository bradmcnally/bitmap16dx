#include "core/settings_view.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace bitmap16 {
namespace SettingsView {

namespace {

struct Item {
  const char* label;
  Action action;
};

constexpr Item kItems[] = {
    {"UI theme", Action::ThemeChanged},
    {"Grid default", Action::DefaultGridChanged},
    {"RGB matrix", Action::MatrixMenuRequested},
    {"Export", Action::ExportFormatChanged},
    {"Shake undo", Action::ShakeUndoChanged},
    {"Save warnings", Action::SaveWarningsChanged},
    {"Indicator LED", Action::IndicatorMenuRequested},
    {"Bluetooth", Action::BluetoothRequested},
    {"Quit", Action::QuitRequested},
};

constexpr Item kMatrixItems[] = {
    {"Enabled", Action::MatrixEnabledChanged},
    {"Layout", Action::MatrixUnitsChanged},
    {"Rotation", Action::MatrixRotationChanged},
    {"Brightness", Action::MatrixBrightnessChanged},
};

constexpr Item kIndicatorItems[] = {
    {"Palette color", Action::IndicatorPaletteChanged},
    {"Low battery", Action::IndicatorLowBatteryChanged},
};

bool itemVisible(
    const Item& item,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit,
    bool includeIndicator) {
  if (item.action == Action::BluetoothRequested) return includeBluetooth;
  if (item.action == Action::QuitRequested) return includeQuit;
  if (item.action == Action::MatrixMenuRequested) {
    return includeMatrix;
  }
  if (item.action == Action::IndicatorMenuRequested) {
    return includeIndicator;
  }
  if (item.action == Action::ShakeUndoChanged) return includeShakeUndo;
  return true;
}

const Item& visibleItem(
    Page page,
    int index,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit,
    bool includeIndicator) {
  if (page == Page::RgbMatrix) {
    const int clamped = std::max(
        0,
        std::min(
            static_cast<int>(
                sizeof(kMatrixItems) / sizeof(kMatrixItems[0])) -
                1,
            index));
    return kMatrixItems[clamped];
  }
  if (page == Page::IndicatorLed) {
    const int clamped = std::max(
        0,
        std::min(
            static_cast<int>(
                sizeof(kIndicatorItems) / sizeof(kIndicatorItems[0])) -
                1,
            index));
    return kIndicatorItems[clamped];
  }
  int visibleIndex = 0;
  for (const Item& item : kItems) {
    if (!itemVisible(
            item,
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit,
            includeIndicator)) {
      continue;
    }
    if (visibleIndex == index) {
      return item;
    }
    ++visibleIndex;
  }
  return kItems[0];
}

const char* valueFor(
    Action action,
    const Settings& settings,
    const char* bluetoothValue,
    char* buffer,
    std::size_t bufferSize) {
  switch (action) {
    case Action::ThemeChanged:
      return settings.theme == ThemeId::Light ? "Light" : "Dark";
    case Action::DefaultGridChanged:
      return settings.defaultGridSize == 8
          ? "8"
          : settings.defaultGridSize == 16 ? "16" : "32";
    case Action::MatrixMenuRequested:
      return settings.matrixEnabled ? "ON" : "OFF";
    case Action::MatrixEnabledChanged:
      return settings.matrixEnabled ? "ON" : "OFF";
    case Action::MatrixUnitsChanged:
      return settings.matrixUnits == 1 ? "1 UNIT" : "4 UNITS";
    case Action::MatrixRotationChanged:
      std::snprintf(
          buffer,
          bufferSize,
          "%u",
          static_cast<unsigned>(settings.matrixRotation) * 90);
      return buffer;
    case Action::MatrixBrightnessChanged:
      std::snprintf(
          buffer,
          bufferSize,
          "%u",
          static_cast<unsigned>(settings.matrixBrightness));
      return buffer;
    case Action::IndicatorMenuRequested:
      return settings.indicatorPaletteColor ? "ON" : "OFF";
    case Action::IndicatorPaletteChanged:
      return settings.indicatorPaletteColor ? "ON" : "OFF";
    case Action::IndicatorLowBatteryChanged:
      return settings.indicatorLowBattery ? "ON" : "OFF";
    case Action::ExportFormatChanged:
      return settings.exportFormat == ExportFormat::Rgb565
          ? "RGB565"
          : "RGB888";
    case Action::ShakeUndoChanged:
      return settings.shakeUndoEnabled ? "ON" : "OFF";
    case Action::SaveWarningsChanged:
      return settings.saveWarnings ? "ON" : "OFF";
    case Action::BluetoothRequested:
      return bluetoothValue == nullptr ? "OFF" : bluetoothValue;
    case Action::QuitRequested:
      return "EXIT";
    default:
      return "";
  }
}

}  // namespace

int itemCount(
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit,
    bool includeIndicator) {
  int count = 0;
  for (const Item& item : kItems) {
    if (itemVisible(
            item,
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit,
            includeIndicator)) {
      ++count;
    }
  }
  return count;
}

bool moveCursor(
    State& state,
    int delta,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit,
    bool includeIndicator) {
  const int maximum = state.page == Page::RgbMatrix
      ? static_cast<int>(
            sizeof(kMatrixItems) / sizeof(kMatrixItems[0])) -
            1
      : state.page == Page::IndicatorLed
          ? static_cast<int>(
                sizeof(kIndicatorItems) / sizeof(kIndicatorItems[0])) -
                1
      : itemCount(
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit,
            includeIndicator) -
            1;
  const int next = std::max(0, std::min(maximum, state.cursor + delta));
  if (next == state.cursor) {
    return false;
  }
  state.cursor = next;
  return true;
}

Action activate(
    State& state,
    Settings& settings,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit,
    bool includeIndicator) {
  const int count = state.page == Page::RgbMatrix
      ? static_cast<int>(
            sizeof(kMatrixItems) / sizeof(kMatrixItems[0]))
      : state.page == Page::IndicatorLed
          ? static_cast<int>(
                sizeof(kIndicatorItems) / sizeof(kIndicatorItems[0]))
      : itemCount(
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit,
            includeIndicator);
  state.cursor =
      std::max(
          0,
          std::min(count - 1, state.cursor));
  const Action action =
      visibleItem(
          state.page,
          state.cursor,
          includeBluetooth,
          includeMatrix,
          includeShakeUndo,
          includeQuit,
          includeIndicator)
          .action;
  switch (action) {
    case Action::ThemeChanged:
      settings.theme = settings.theme == ThemeId::Light
          ? ThemeId::Dark
          : ThemeId::Light;
      return Action::ThemeChanged;
    case Action::DefaultGridChanged:
      settings.defaultGridSize =
          settings.defaultGridSize == 8
              ? 16
              : settings.defaultGridSize == 16 ? 32 : 8;
      return Action::DefaultGridChanged;
    case Action::MatrixMenuRequested:
      state.page = Page::RgbMatrix;
      state.cursor = 0;
      state.scrollOffset = 0;
      return Action::MatrixMenuRequested;
    case Action::MatrixEnabledChanged:
      settings.matrixEnabled = !settings.matrixEnabled;
      return Action::MatrixEnabledChanged;
    case Action::MatrixUnitsChanged:
      settings.matrixUnits = settings.matrixUnits == 1 ? 4 : 1;
      return Action::MatrixUnitsChanged;
    case Action::MatrixRotationChanged:
      settings.matrixRotation =
          static_cast<uint8_t>((settings.matrixRotation + 1) % 4);
      return Action::MatrixRotationChanged;
    case Action::MatrixBrightnessChanged:
      settings.matrixBrightness =
          settings.matrixBrightness >= 20
              ? 1
              : static_cast<uint8_t>(settings.matrixBrightness + 1);
      return Action::MatrixBrightnessChanged;
    case Action::IndicatorMenuRequested:
      state.page = Page::IndicatorLed;
      state.cursor = 0;
      state.scrollOffset = 0;
      return Action::IndicatorMenuRequested;
    case Action::IndicatorPaletteChanged:
      settings.indicatorPaletteColor = !settings.indicatorPaletteColor;
      return Action::IndicatorPaletteChanged;
    case Action::IndicatorLowBatteryChanged:
      settings.indicatorLowBattery = !settings.indicatorLowBattery;
      return Action::IndicatorLowBatteryChanged;
    case Action::ExportFormatChanged:
      settings.exportFormat =
          settings.exportFormat == ExportFormat::Rgb888
              ? ExportFormat::Rgb565
              : ExportFormat::Rgb888;
      return Action::ExportFormatChanged;
    case Action::ShakeUndoChanged:
      settings.shakeUndoEnabled = !settings.shakeUndoEnabled;
      return Action::ShakeUndoChanged;
    case Action::SaveWarningsChanged:
      settings.saveWarnings = !settings.saveWarnings;
      return Action::SaveWarningsChanged;
    case Action::BluetoothRequested:
      return includeBluetooth ? Action::BluetoothRequested : Action::None;
    case Action::QuitRequested:
      return includeQuit ? Action::QuitRequested : Action::None;
    default:
      return Action::None;
  }
}

void render(
    Canvas& canvas,
    State& state,
    const Settings& settings,
    const Theme& theme,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    const char* bluetoothValue,
    const char* statusMessage,
    bool includeQuit,
    bool includeIndicator) {
  if (!canvas.isValid()) {
    return;
  }

  canvas.fillScreen(theme.background);
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(theme.text);
  canvas.drawString(
      state.page == Page::RgbMatrix
          ? "RGB MATRIX"
          : state.page == Page::IndicatorLed ? "INDICATOR LED" : "SETTINGS",
      4,
      4);

  const int totalItems = state.page == Page::RgbMatrix
      ? static_cast<int>(
            sizeof(kMatrixItems) / sizeof(kMatrixItems[0]))
      : state.page == Page::IndicatorLed
          ? static_cast<int>(
                sizeof(kIndicatorItems) / sizeof(kIndicatorItems[0]))
      : itemCount(
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit,
            includeIndicator);
  state.cursor = std::max(0, std::min(totalItems - 1, state.cursor));
  state.scrollOffset =
      std::max(0, std::min(state.cursor, state.scrollOffset));

  constexpr int startY = 18;
  constexpr int lineHeight = 16;
  constexpr int selectedLineHeight = 28;
  const int labelX = std::max(12, canvas.width() / 20);
  const int valueRight = canvas.width() - std::max(12, canvas.width() / 20);
  // Let the next row extend to the physical screen edge. The canvas clips it
  // naturally, leaving a visible hint that more settings continue below.
  const int contentBottom = canvas.height();

  while (state.scrollOffset < state.cursor) {
    int y = startY;
    for (int i = state.scrollOffset; i <= state.cursor; ++i) {
      y += i == state.cursor ? selectedLineHeight : lineHeight;
    }
    if (y <= contentBottom) {
      break;
    }
    ++state.scrollOffset;
  }

  int y = startY;
  char valueBuffer[16] = {};
  char selectedValue[20] = {};
  for (int i = state.scrollOffset; i < totalItems; ++i) {
    if (y >= contentBottom) {
      break;
    }
    const bool selected = i == state.cursor;
    const uint8_t textSize = selected ? 2 : 1;
    const int textY = selected ? y + 4 : y;
    canvas.setTextSize(textSize);
    canvas.setTextColor(selected ? theme.text : theme.textSecondary);
    canvas.setTextAlign(TextAlign::Left);
    const Item item = visibleItem(
        state.page,
        i,
        includeBluetooth,
        includeMatrix,
        includeShakeUndo,
        includeQuit,
        includeIndicator);
    canvas.drawString(
        item.label,
        labelX,
        textY);

    const char* value = valueFor(
        item.action,
        settings,
        bluetoothValue,
        valueBuffer,
        sizeof(valueBuffer));
    if (item.action == Action::MatrixMenuRequested ||
        item.action == Action::IndicatorMenuRequested) {
      std::snprintf(
          selectedValue, sizeof(selectedValue), "%s >", value);
      value = selectedValue;
    } else if (selected) {
      std::snprintf(
          selectedValue, sizeof(selectedValue), "<%s>", value);
      value = selectedValue;
    }
    canvas.setTextAlign(TextAlign::Right);
    canvas.drawString(value, valueRight, textY);
    y += selected ? selectedLineHeight : lineHeight;
  }

  if (statusMessage != nullptr && statusMessage[0] != '\0') {
    canvas.setTextAlign(TextAlign::Left);
    canvas.setTextSize(1);
    canvas.setTextColor(theme.text);
    canvas.drawString(statusMessage, 3, canvas.height() - 11);
  }
}

}  // namespace SettingsView
}  // namespace bitmap16
