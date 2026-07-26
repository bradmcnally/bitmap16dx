#include "core/help_view.h"

#include <algorithm>

namespace bitmap16 {
namespace HelpView {

namespace {

struct Item {
  const char* label;
  const char* key;
  int group;
  bool requiresLedMatrix;
  bool requiresBattery;
};

constexpr Item kItems[] = {
    {"Cursor", "Arrows", 0, false, false},
    {"Draw", "Ok", 0, false, false},
    {"Erase", "Del", 0, false, false},
    {"Fill", "F", 0, false, false},
    {"Move", "M arrows", 0, false, false},
    {"Color 1-8", "1-8", 0, false, false},
    {"Color 9-16", "Fn 1-8", 0, false, false},
    {"Palette", "P", 0, false, false},
    {"Clear", "G0", 0, false, false},
    {"Preview", "V", 0, false, false},
    {"Grid size", "G", 0, false, false},
    {"Grid ruler", "R", 0, false, false},
    {"Open", "O", 1, false, false},
    {"Undo", "Z", 1, false, false},
    {"Redo", "Fn Z", 1, false, false},
    {"Save", "S", 1, false, false},
    {"Save as", "Fn S", 1, false, false},
    {"Settings", "T", 1, false, false},
    {"Export", "X", 1, false, false},
    {"Brightness", "B +/-", 1, false, false},
    {"Charge", "Fn B", 1, false, true},
    {"RGB on/off", "L Ok", 2, true, false},
    {"RGB Bright", "L +/-", 2, true, false},
};

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
    bool includeBatteryControls) {
  if (!canvas.isValid()) {
    return;
  }

  canvas.fillScreen(theme.background);
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(theme.text);
  canvas.drawString("HELP", 4, 4);

  const int totalItems =
      itemCount(includeLedMatrixControls, includeBatteryControls);
  state.cursor = std::max(0, std::min(totalItems - 1, state.cursor));
  state.scrollOffset =
      std::max(0, std::min(state.cursor, state.scrollOffset));

  constexpr int startY = 18;
  constexpr int lineHeight = 14;
  constexpr int selectedLineHeight = 24;
  constexpr int groupGap = 8;
  const int labelX = std::max(12, canvas.width() / 20);
  const int keyX = canvas.width() * 13 / 20;

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
    canvas.drawString(item.key, keyX, textY);
    y += selected ? selectedLineHeight : lineHeight;
  }
}

}  // namespace HelpView
}  // namespace bitmap16
