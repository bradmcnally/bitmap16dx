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
    {"RGB matrix", Action::MatrixUnitsChanged},
    {"Rotate matrix", Action::MatrixRotationChanged},
    {"Export", Action::ExportFormatChanged},
    {"Shake undo", Action::ShakeUndoChanged},
    {"Bluetooth", Action::BluetoothRequested},
    {"Quit", Action::QuitRequested},
};

bool itemVisible(
    const Item& item,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit) {
  if (item.action == Action::BluetoothRequested) return includeBluetooth;
  if (item.action == Action::QuitRequested) return includeQuit;
  if (item.action == Action::MatrixUnitsChanged ||
      item.action == Action::MatrixRotationChanged) {
    return includeMatrix;
  }
  if (item.action == Action::ShakeUndoChanged) return includeShakeUndo;
  return true;
}

const Item& visibleItem(
    int index,
    bool includeBluetooth,
    bool includeMatrix,
    bool includeShakeUndo,
    bool includeQuit) {
  int visibleIndex = 0;
  for (const Item& item : kItems) {
    if (!itemVisible(
            item,
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit)) {
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
    case Action::MatrixUnitsChanged:
      return settings.matrixUnits == 1 ? "1" : "4";
    case Action::MatrixRotationChanged:
      std::snprintf(
          buffer,
          bufferSize,
          "%u",
          static_cast<unsigned>(settings.matrixRotation) * 90);
      return buffer;
    case Action::ExportFormatChanged:
      return settings.exportFormat == ExportFormat::Rgb565
          ? "RGB565"
          : "RGB888";
    case Action::ShakeUndoChanged:
      return settings.shakeUndoEnabled ? "ON" : "OFF";
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
    bool includeQuit) {
  int count = 0;
  for (const Item& item : kItems) {
    if (itemVisible(
            item,
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit)) {
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
    bool includeQuit) {
  const int maximum =
      itemCount(
          includeBluetooth, includeMatrix, includeShakeUndo, includeQuit) - 1;
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
    bool includeQuit) {
  state.cursor =
      std::max(
          0,
          std::min(
              itemCount(
                  includeBluetooth,
                  includeMatrix,
                  includeShakeUndo,
                  includeQuit) -
                  1,
              state.cursor));
  const Action action =
      visibleItem(
          state.cursor,
          includeBluetooth,
          includeMatrix,
          includeShakeUndo,
          includeQuit)
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
    case Action::MatrixUnitsChanged:
      settings.matrixUnits = settings.matrixUnits == 1 ? 4 : 1;
      return Action::MatrixUnitsChanged;
    case Action::MatrixRotationChanged:
      settings.matrixRotation =
          static_cast<uint8_t>((settings.matrixRotation + 1) % 4);
      return Action::MatrixRotationChanged;
    case Action::ExportFormatChanged:
      settings.exportFormat =
          settings.exportFormat == ExportFormat::Rgb888
              ? ExportFormat::Rgb565
              : ExportFormat::Rgb888;
      return Action::ExportFormatChanged;
    case Action::ShakeUndoChanged:
      settings.shakeUndoEnabled = !settings.shakeUndoEnabled;
      return Action::ShakeUndoChanged;
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
    bool includeQuit) {
  if (!canvas.isValid()) {
    return;
  }

  canvas.fillScreen(theme.background);
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(theme.text);
  canvas.drawString("SETTINGS", 4, 4);

  const int totalItems =
      itemCount(
          includeBluetooth, includeMatrix, includeShakeUndo, includeQuit);
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
        visibleItem(
            i,
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit)
            .label,
        labelX,
        textY);

    const char* value = valueFor(
        visibleItem(
            i,
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit)
            .action,
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
