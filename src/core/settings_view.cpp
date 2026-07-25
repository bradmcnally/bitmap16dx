#include "core/settings_view.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace bitmap16 {
namespace SettingsView {

namespace {

struct Item {
  const char* label;
  bool requiresBluetooth;
};

constexpr Item kItems[] = {
    {"UI theme", false},
    {"Grid default", false},
    {"RGB matrix", false},
    {"Rotate matrix", false},
    {"Export", false},
    {"Shake undo", false},
    {"Bluetooth", true},
};

const Item& visibleItem(int index, bool includeBluetooth) {
  int visibleIndex = 0;
  for (const Item& item : kItems) {
    if (!includeBluetooth && item.requiresBluetooth) {
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
    int index,
    const Settings& settings,
    const char* bluetoothValue,
    char* buffer,
    std::size_t bufferSize) {
  switch (index) {
    case 0:
      return settings.theme == ThemeId::Light ? "Light" : "Dark";
    case 1:
      return settings.defaultGridSize == 8 ? "8" : "16";
    case 2:
      return settings.matrixUnits == 1 ? "1" : "4";
    case 3:
      std::snprintf(
          buffer,
          bufferSize,
          "%u",
          static_cast<unsigned>(settings.matrixRotation) * 90);
      return buffer;
    case 4:
      return settings.exportFormat == ExportFormat::Rgb565
          ? "RGB565"
          : "RGB888";
    case 5:
      return settings.shakeUndoEnabled ? "ON" : "OFF";
    case 6:
      return bluetoothValue == nullptr ? "OFF" : bluetoothValue;
    default:
      return "";
  }
}

}  // namespace

int itemCount(bool includeBluetooth) {
  int count = 0;
  for (const Item& item : kItems) {
    if (includeBluetooth || !item.requiresBluetooth) {
      ++count;
    }
  }
  return count;
}

bool moveCursor(State& state, int delta, bool includeBluetooth) {
  const int maximum = itemCount(includeBluetooth) - 1;
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
    bool includeBluetooth) {
  state.cursor =
      std::max(0, std::min(itemCount(includeBluetooth) - 1, state.cursor));
  switch (state.cursor) {
    case 0:
      settings.theme = settings.theme == ThemeId::Light
          ? ThemeId::Dark
          : ThemeId::Light;
      return Action::ThemeChanged;
    case 1:
      settings.defaultGridSize =
          settings.defaultGridSize == 8 ? 16 : 8;
      return Action::DefaultGridChanged;
    case 2:
      settings.matrixUnits = settings.matrixUnits == 1 ? 4 : 1;
      return Action::MatrixUnitsChanged;
    case 3:
      settings.matrixRotation =
          static_cast<uint8_t>((settings.matrixRotation + 1) % 4);
      return Action::MatrixRotationChanged;
    case 4:
      settings.exportFormat =
          settings.exportFormat == ExportFormat::Rgb888
              ? ExportFormat::Rgb565
              : ExportFormat::Rgb888;
      return Action::ExportFormatChanged;
    case 5:
      settings.shakeUndoEnabled = !settings.shakeUndoEnabled;
      return Action::ShakeUndoChanged;
    case 6:
      return includeBluetooth ? Action::BluetoothRequested : Action::None;
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
    const char* bluetoothValue,
    const char* statusMessage) {
  if (!canvas.isValid()) {
    return;
  }

  canvas.fillScreen(theme.background);
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(theme.text);
  canvas.drawString("SETTINGS", 4, 4);

  const int totalItems = itemCount(includeBluetooth);
  state.cursor = std::max(0, std::min(totalItems - 1, state.cursor));
  state.scrollOffset =
      std::max(0, std::min(state.cursor, state.scrollOffset));

  constexpr int startY = 18;
  constexpr int lineHeight = 16;
  constexpr int selectedLineHeight = 28;
  const int labelX = std::max(12, canvas.width() / 20);
  const int valueRight = canvas.width() - std::max(12, canvas.width() / 20);
  const int contentBottom = canvas.height() - 12;

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
    canvas.drawString(
        visibleItem(i, includeBluetooth).label,
        labelX,
        textY);

    const char* value = valueFor(
        i,
        settings,
        bluetoothValue,
        valueBuffer,
        sizeof(valueBuffer));
    if (selected) {
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
