#include "core/help_view.h"

#include <algorithm>
#include <cstring>

namespace bitmap16 {
namespace HelpView {

namespace {

struct Item {
  const char* label;
  const char* key;
  const char* zeroKey;
  int group;
  bool requiresLedMatrix;
  bool requiresBattery;
};

constexpr Item kItems[] = {
    {"Cursor", "Arrows", "F Z X C", 0, false, false},
    {"Draw", "Ok", "Enter", 0, false, false},
    {"Erase", "Del", "Backspace", 0, false, false},
    {"Fill", "F", "Ctrl F", 0, false, false},
    {"Erase fill", "Fn F", "E", 0, false, false},
    {"Move", "M arrows", "M + FZXC", 0, false, false},
    {"Undo", "Z", "Ctrl Z", 0, false, false},
    {"Redo", "Fn Z", "Ctrl Y", 0, false, false},
    {"Clear", "G0", "Fn Bksp", 0, false, false},
#ifndef BITMAP16_STEAM_DECK
    {"Zoom", "+/-", "Fn+L/M", 1, false, false},
#endif
    {"Edge jump", "Fn edge", "Shift dir", 1, false, false},
    {"Grid size", "G", "G", 1, false, false},
    {"Grid ruler", "R", "R", 1, false, false},
    {"Color 1-8", "1-8", "1-8", 2, false, false},
    {"Color 9-16", "Fn 1-8", "Fn 1-8", 2, false, false},
    {"Next color", "C", "I", 2, false, false},
    {"Palette", "P", "P", 2, false, false},
#ifdef BITMAP16_STEAM_DECK
    {"Pal range", "LB/RB", "", 2, false, false},
    {"User palettes", "Dpad Up", "", 2, false, false},
    {"Clear filter", "Dpad Down", "", 2, false, false},
#endif
    {"Open", "O", "O", 3, false, false},
    {"Save", "S", "Ctrl S", 3, false, false},
    {"Save as", "Fn S", "Shift S", 3, false, false},
    {"Export", "X", "Ctrl X", 3, false, false},
    {"Native PNG", "Fn X", "Ctrl L", 3, false, false},
    {"Preview", "V", "V", 4, false, false},
    {"Settings", "T", "T", 4, false, false},
    {"Screensaver", "Fn B", "Ctrl B", 4, false, true},
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    {"Brightness", "B +/-", "Fn U/I", 5, false, false},
    {"Home", "", "Esc 3s", 5, false, false},
#else
    {"Brightness", "B +/-", "B +/-", 5, false, false},
#endif
    {"RGB on/off", "L Ok", "L Enter", 5, true, false},
    {"RGB Bright", "L +/-", "L +/-", 5, true, false},
};

constexpr const char* kGroupNames[] = {
    "DRAW", "CANVAS", "COLOR", "SKETCHES & FILES", "VIEWS", "DEVICE"};

const Item& visibleItem(
    int index,
    bool includeLedMatrixControls,
    bool includeBatteryControls) {
  int visibleIndex = 0;
  for (const Item& item : kItems) {
    if (!includeLedMatrixControls && item.requiresLedMatrix) {
      continue;
    }
    if (!includeBatteryControls && item.requiresBattery) continue;
    if (visibleIndex == index) {
      return item;
    }
    ++visibleIndex;
  }
  return kItems[0];
}

}  // namespace

int itemCount(
    bool includeLedMatrixControls, bool includeBatteryControls) {
  int count = 0;
  for (const Item& item : kItems) {
    if ((includeLedMatrixControls || !item.requiresLedMatrix) &&
        (includeBatteryControls || !item.requiresBattery)) {
      ++count;
    }
  }
  return count;
}

bool moveCursor(
    State& state,
    int delta,
    bool includeLedMatrixControls,
    bool includeBatteryControls) {
  const int maximum =
      itemCount(includeLedMatrixControls, includeBatteryControls) - 1;
  const int next = std::max(0, std::min(maximum, state.cursor + delta));
  if (next == state.cursor) {
    return false;
  }
  state.cursor = next;
  return true;
}

void render(
    Canvas& canvas,
    State& state,
    const Theme& theme,
    bool includeLedMatrixControls,
    bool includeBatteryControls,
    bool includeCtrlDraw) {
#ifdef BITMAP16_CARDPUTER_ZERO
  (void)includeCtrlDraw;
#endif
  if (!canvas.isValid()) {
    return;
  }

  canvas.fillScreen(theme.background);
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(theme.text);
  const int totalItems =
      itemCount(includeLedMatrixControls, includeBatteryControls);
  state.cursor = std::max(0, std::min(totalItems - 1, state.cursor));
  state.scrollOffset =
      std::max(0, std::min(state.cursor, state.scrollOffset));
  canvas.drawString("HELP > ", 4, 4);
  canvas.drawString(
      kGroupNames[visibleItem(
          state.cursor, includeLedMatrixControls, includeBatteryControls).group],
      4 + 7 * 6, 4);

  constexpr int startY = 18;
  constexpr int lineHeight = 14;
  constexpr int selectedLineHeight = 24;
  constexpr int groupGap = 8;
  const int labelX = std::max(12, canvas.width() / 20);
  const int keyX = canvas.width() * 13 / 20 - 16;

  if (state.cursor < state.scrollOffset) {
    state.scrollOffset = state.cursor;
  }
  while (state.scrollOffset < state.cursor) {
    int y = startY;
    for (int i = state.scrollOffset; i <= state.cursor; ++i) {
      const Item& item = visibleItem(
          i, includeLedMatrixControls, includeBatteryControls);
      if (i > state.scrollOffset) {
        const Item& previous =
            visibleItem(
                i - 1, includeLedMatrixControls, includeBatteryControls);
        if (item.group != previous.group) {
          y += groupGap;
        }
      }
      y += i == state.cursor ? selectedLineHeight : lineHeight;
    }
    if (y <= canvas.height()) {
      break;
    }
    ++state.scrollOffset;
  }

  int y = startY;
  for (int i = state.scrollOffset; i < totalItems; ++i) {
    const Item& item = visibleItem(
        i, includeLedMatrixControls, includeBatteryControls);
    if (i > state.scrollOffset) {
      const Item& previous =
          visibleItem(
              i - 1, includeLedMatrixControls, includeBatteryControls);
      if (item.group != previous.group) {
        y += groupGap;
      }
    }
    if (y >= canvas.height()) {
      break;
    }

    const bool selected = i == state.cursor;
    const uint8_t textSize = selected ? 2 : 1;
    const int textY = selected ? y + 4 : y;
    canvas.setTextSize(textSize);
    canvas.setTextColor(selected ? theme.text : theme.textSecondary);
    canvas.drawString(item.label, labelX, textY);
    const char* keyLabel = item.key;
#ifdef BITMAP16_CARDPUTER_ZERO
    keyLabel = item.zeroKey;
#else
    if (includeCtrlDraw && std::strcmp(item.label, "Draw") == 0) {
      keyLabel = "Ok/Ctrl";
    }
#endif
    canvas.drawString(keyLabel, keyX, textY);
    y += selected ? selectedLineHeight : lineHeight;
  }
}

}  // namespace HelpView
}  // namespace bitmap16
