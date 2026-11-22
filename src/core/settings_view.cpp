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
#ifdef BITMAP16_CARDPUTER_ZERO
    {"Battery display", Action::BatteryDisplayChanged},
#endif
#if defined(BITMAP16_CARDPUTER_ZERO_DEVICE) && defined(BITMAP16_BETA_SETTINGS)
    {"Warmth", Action::DisplayWarmthChanged},
#endif
    {"Cursor", Action::CursorStyleChanged},
    {"Audio", Action::AudioMenuRequested},
    {"Bluetooth", Action::BluetoothRequested},
    {"Help", Action::HelpRequested},
    {"Quit", Action::QuitRequested},
};

constexpr Item kAudioItems[] = {
    {"Sound effects", Action::SoundEffectsChanged},
    {"Music", Action::MusicEnabledChanged},
    {"Music volume", Action::MusicVolumeChanged},
    {"Sound effects volume", Action::SoundEffectsVolumeChanged},
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
    bool includeIndicator,
    bool includeHelp,
    bool includeAudio) {
  if (item.action == Action::AudioMenuRequested) return includeAudio;
  if (item.action == Action::BluetoothRequested) return includeBluetooth;
  if (item.action == Action::HelpRequested) return includeHelp;
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
    bool includeIndicator,
    bool includeHelp,
    bool includeAudio) {
  if (page == Page::Audio) {
    return kAudioItems[std::clamp(index, 0, static_cast<int>(sizeof(kAudioItems) / sizeof(kAudioItems[0])) - 1)];
  }
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
            includeIndicator,
            includeHelp, includeAudio)) {
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
    case Action::AudioMenuRequested:
      return "OPEN";
    case Action::SoundEffectsChanged:
      return settings.soundEffectsEnabled ? "ON" : "OFF";
    case Action::MusicEnabledChanged:
      return settings.musicEnabled ? "ON" : "OFF";
    case Action::SoundEffectsVolumeChanged:
    case Action::MusicVolumeChanged:
      std::snprintf(buffer, bufferSize, "%u%%", static_cast<unsigned>(
          action == Action::MusicVolumeChanged ? settings.musicVolume : settings.soundEffectsVolume));
      return buffer;
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
    case Action::BatteryDisplayChanged:
      return settings.batteryDisplay ? "ON" : "OFF";
    case Action::DisplayWarmthChanged:
      switch (settings.displayWarmth) {
        case 1: return "Mild";
        case 2: return "Warm";
        case 3: return "Warmer";
        default: return "Neutral";
      }
    case Action::CursorStyleChanged:
      return settings.cursorStyle == CursorStyle::Hand ? "HAND" : "ARROW";
    case Action::BluetoothRequested:
      return bluetoothValue == nullptr ? "OFF" : bluetoothValue;
    case Action::HelpRequested:
      return "OPEN";
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
    bool includeIndicator,
    bool includeHelp,
    bool includeAudio) {
  int count = 0;
  for (const Item& item : kItems) {
    if (itemVisible(
            item,
            includeBluetooth,
            includeMatrix,
            includeShakeUndo,
            includeQuit,
            includeIndicator,
            includeHelp, includeAudio)) {
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
    bool includeIndicator,
    bool includeHelp,
    bool includeAudio) {
  const int maximum = state.page == Page::Audio
      ? static_cast<int>(sizeof(kAudioItems) / sizeof(kAudioItems[0])) - 1
      : state.page == Page::RgbMatrix
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
            includeIndicator,
            includeHelp, includeAudio) -
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
    bool includeIndicator,
    bool includeHelp,
    bool includeAudio,
    int direction) {
  const int count = state.page == Page::Audio
      ? static_cast<int>(sizeof(kAudioItems) / sizeof(kAudioItems[0]))
      : state.page == Page::RgbMatrix
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
            includeIndicator,
            includeHelp, includeAudio);
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
          includeIndicator,
          includeHelp, includeAudio)
          .action;
  switch (action) {
    case Action::AudioMenuRequested:
      state.parentCursor = state.cursor;
      state.page = Page::Audio;
      state.cursor = 0;
      state.scrollOffset = 0;
      return action;
    case Action::SoundEffectsChanged:
      settings.soundEffectsEnabled = !settings.soundEffectsEnabled;
      return action;
    case Action::MusicEnabledChanged:
      settings.musicEnabled = !settings.musicEnabled;
      return action;
    case Action::SoundEffectsVolumeChanged:
    case Action::MusicVolumeChanged: {
      uint8_t& percent = action == Action::MusicVolumeChanged
          ? settings.musicVolume : settings.soundEffectsVolume;
      const auto next = static_cast<uint8_t>(std::clamp<int>(
          percent + (direction < 0 ? -10 : 10), 0, 100));
      if (next == percent) return Action::None;
      percent = next;
      return action;
    }
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
    case Action::BatteryDisplayChanged:
      settings.batteryDisplay = !settings.batteryDisplay;
      return action;
    case Action::DisplayWarmthChanged:
      settings.displayWarmth = static_cast<uint8_t>(
          (settings.displayWarmth + 1) % 4);
      return action;
    case Action::CursorStyleChanged:
      settings.cursorStyle = settings.cursorStyle == CursorStyle::Arrow
          ? CursorStyle::Hand
          : CursorStyle::Arrow;
      return Action::CursorStyleChanged;
    case Action::BluetoothRequested:
      return includeBluetooth ? Action::BluetoothRequested : Action::None;
    case Action::HelpRequested:
      return includeHelp ? Action::HelpRequested : Action::None;
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
    bool includeIndicator,
    bool includeHelp,
    bool includeAudio) {
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
          : state.page == Page::IndicatorLed ? "INDICATOR LED"
          : state.page == Page::Audio ? "AUDIO" : "SETTINGS",
      4,
      4);

  const int totalItems = state.page == Page::Audio
      ? static_cast<int>(sizeof(kAudioItems) / sizeof(kAudioItems[0]))
      : state.page == Page::RgbMatrix
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
            includeIndicator,
            includeHelp, includeAudio);
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
        includeIndicator,
        includeHelp, includeAudio);
    const char* value = valueFor(
        item.action,
        settings,
        bluetoothValue,
        valueBuffer,
        sizeof(valueBuffer));
    if (item.action == Action::AudioMenuRequested ||
        item.action == Action::MatrixMenuRequested ||
        item.action == Action::IndicatorMenuRequested ||
        item.action == Action::HelpRequested) {
      std::snprintf(
          selectedValue, sizeof(selectedValue), "%s >", value);
      value = selectedValue;
    } else if (selected) {
      std::snprintf(
          selectedValue, sizeof(selectedValue), "<%s>", value);
      value = selectedValue;
    }
    // Keep full labels readable beside enlarged values on smaller displays.
    const int valueWidth = canvas.textWidth(value);
    const bool compactLabel = selected &&
        canvas.textWidth(item.label) + valueWidth + 8 > valueRight - labelX;
    if (compactLabel) canvas.setTextSize(1);
    canvas.drawString(item.label, labelX, textY + (compactLabel ? 4 : 0));
    canvas.setTextSize(textSize);
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
