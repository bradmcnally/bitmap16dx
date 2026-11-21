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
  const char* zeroExternalKey = nullptr;
};

constexpr Item kItems[] = {
#ifdef BITMAP16_STEAM_DECK
    {"Cursor", "L stick", "", 0, false, false},
    {"Draw", "A", "", 0, false, false},
    {"Erase", "X", "", 0, false, false},
    {"Fill", "Y", "", 0, false, false},
    {"Move", "LT+L stick", "", 0, false, false},
    {"Undo", "LB", "", 0, false, false},
    {"Redo", "RB", "", 0, false, false},
    {"Resize", "L3 / L4", "", 1, false, false},
    {"Rulers", "R5", "", 1, false, false},
    {"Palettes", "R3", "", 2, false, false},
    {"Color", "R stick", "", 2, false, false},
    {"Browse", "L/R stick", "", 2, false, false},
    {"Apply", "A", "", 2, false, false},
    {"Lospec", "Y", "", 2, false, false},
    {"Import", "L3", "", 2, false, false},
    {"Color range", "LB/RB", "", 2, false, false},
    {"User palettes", "Dpad Up", "", 2, false, false},
    {"All palettes", "Dpad Down", "", 2, false, false},
    {"Retry feed", "X", "", 2, false, false},
    {"Sketches", "View", "", 3, false, false},
    {"Open", "A", "", 3, false, false},
    {"Save", "LT+RT", "", 3, false, false},
    {"Duplicate", "Y", "", 3, false, false},
    {"Delete", "X", "", 3, false, false},
    {"Restore", "LB", "", 3, false, false},
    {"Preview", "L5", "", 4, false, false},
    {"Browse", "L stick", "", 4, false, false},
    {"Zoom", "LT/RT", "", 4, false, false},
    {"Background", "A/X/Y", "", 4, false, false},
    {"Settings", "Menu", "", 5, false, false},
    {"Help", "R4", "", 5, false, false},
    {"Back", "B", "", 5, false, false},
    {"Quit", "Settings", "", 5, false, false},
    {"Clear", "K", "", 6, false, false},
    {"Erase fill", "Alt F", "", 6, false, false},
    {"Save as", "Alt S", "", 6, false, false},
    {"Export", "X", "", 6, false, false},
    {"Native PNG", "Alt X", "", 6, false, false},
    {"Screenshot", "F12", "", 6, false, false},
    {"Autoplay", "Space", "", 6, false, false},
#else
    {"Cursor", "Arrows", "F Z X C", 0, false, false, "Arrows"},
    {"Draw", "Ok", "Enter/Spc", 0, false, false},
    {"Erase", "Del", "Backspace", 0, false, false, "Bksp/Del"},
    {"Fill", "F", "Ctrl F", 0, false, false},
    {"Erase fill", "Fn F", "E", 0, false, false},
    {"Move", "M arrows", "M + FZXC", 0, false, false, "M arrows"},
    {"Undo", "Z", "Ctrl Z", 0, false, false},
    {"Redo", "Fn Z", "Ctrl Y", 0, false, false, "Ctrl Sh Z"},
    {"Clear", "G0", "Fn Bksp", 0, false, false, "On device"},
    {"Zoom", "+/-", "Fn+L/M", 1, false, false, "+/-"},
    {"Edge jump", "Fn edge", "Shift dir", 1, false, false},
    {"Grid size", "G", "G", 1, false, false},
    {"Grid ruler", "R", "R", 1, false, false},
    {"Color 1-8", "1-8", "1-8", 2, false, false},
    {"Color 9-16", "Fn 1-8", "Fn 1-8", 2, false, false, "F1-F8"},
    {"Next color", "C", "I", 2, false, false},
    {"Palette", "P", "P", 2, false, false},
    {"Open", "O", "O", 3, false, false},
    {"Save", "S", "Ctrl S", 3, false, false},
    {"Save as", "Fn S", "Shift S", 3, false, false},
    {"Export", "X", "Ctrl X", 3, false, false},
    {"Native PNG", "Fn X", "Ctrl L", 3, false, false},
    {"Preview", "V", "V", 4, false, false},
    {"Settings", "T", "T", 4, false, false},
    {"Screensaver", "Fn B", "Ctrl B", 4, false, true},
#ifdef BITMAP16_CARDPUTER_ZERO_DEVICE
    {"Brightness", "B +/-", "Fn U/I", 5, false, false, "On device"},
    {"Home", "", "Esc 3s", 5, false, false},
#else
    {"Brightness", "B +/-", "B +/-", 5, false, false},
#endif
    {"RGB on/off", "L Ok", "L Enter", 5, true, false},
    {"RGB Bright", "L +/-", "L +/-", 5, true, false},
#endif
};

constexpr const char* kGroupNames[] = {
#ifdef BITMAP16_STEAM_DECK
    "DRAW", "CANVAS", "PALETTES", "SKETCHES", "PREVIEW", "APP", "KEYBOARD"
#else
    "DRAW", "CANVAS", "COLOR", "SKETCHES & FILES", "VIEWS", "DEVICE"
#endif
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

bool moveGroup(
    State& state,
    int delta,
    bool includeLedMatrixControls,
    bool includeBatteryControls) {
  const int count = itemCount(includeLedMatrixControls, includeBatteryControls);
  if (count == 0 || delta == 0) return false;
  const int cursor = std::max(0, std::min(count - 1, state.cursor));
  const int group = visibleItem(cursor, includeLedMatrixControls, includeBatteryControls).group;
  const int direction = delta < 0 ? -1 : 1;
  int next = cursor;
  while (next >= 0 && next < count &&
         visibleItem(next, includeLedMatrixControls, includeBatteryControls).group == group)
    next += direction;
  if (next < 0 || next >= count) return false;
  const int nextGroup = visibleItem(next, includeLedMatrixControls, includeBatteryControls).group;
  while (next > 0 &&
         visibleItem(next - 1, includeLedMatrixControls, includeBatteryControls).group == nextGroup)
    --next;
  state.cursor = state.scrollOffset = next;
  return true;
}

void render(
    Canvas& canvas,
    State& state,
    const Theme& theme,
    bool includeLedMatrixControls,
    bool includeBatteryControls,
    bool includeCtrlDraw) {
#if defined(BITMAP16_CARDPUTER_ZERO) || defined(BITMAP16_STEAM_DECK)
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
  const char* header = "HELP > ";
#ifdef BITMAP16_CARDPUTER_ZERO
  if (state.externalKeyboard) header = "HELP USB > ";
#endif
  canvas.drawString(header, 4, 4);
  canvas.drawString(
      kGroupNames[visibleItem(
          state.cursor, includeLedMatrixControls, includeBatteryControls).group],
      4 + static_cast<int>(std::strlen(header)) * 6, 4);

  int sectionEnd = totalItems;
#ifdef BITMAP16_STEAM_DECK
  const int group = visibleItem(state.cursor, includeLedMatrixControls, includeBatteryControls).group;
  int sectionStart = state.cursor;
  while (sectionStart > 0 &&
         visibleItem(sectionStart - 1, includeLedMatrixControls, includeBatteryControls).group == group)
    --sectionStart;
  sectionEnd = state.cursor + 1;
  while (sectionEnd < totalItems &&
         visibleItem(sectionEnd, includeLedMatrixControls, includeBatteryControls).group == group)
    ++sectionEnd;
  state.scrollOffset = std::max(sectionStart, state.scrollOffset);
  const int contentBottom = canvas.height() - 14;
#else
  const int contentBottom = canvas.height();
#endif
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
    if (y <= contentBottom) {
      break;
    }
    ++state.scrollOffset;
  }

  int y = startY;
  for (int i = state.scrollOffset; i < sectionEnd; ++i) {
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
    const bool selected = i == state.cursor;
#ifdef BITMAP16_STEAM_DECK
    if (y + (selected ? selectedLineHeight : lineHeight) > contentBottom) {
#else
    if (y >= contentBottom) {
#endif
      break;
    }

    const uint8_t textSize = selected ? 2 : 1;
    const int textY = selected ? y + 4 : y;
    canvas.setTextSize(textSize);
    canvas.setTextColor(selected ? theme.text : theme.textSecondary);
    canvas.drawString(item.label, labelX, textY);
    const char* keyLabel = item.key;
#ifdef BITMAP16_CARDPUTER_ZERO
    keyLabel = state.externalKeyboard && item.zeroExternalKey != nullptr
        ? item.zeroExternalKey : item.zeroKey;
#elif !defined(BITMAP16_STEAM_DECK)
    if (includeCtrlDraw && std::strcmp(item.label, "Draw") == 0) {
      keyLabel = "Ok/Ctrl";
    }
#endif
    canvas.drawString(keyLabel, keyX, textY);
    y += selected ? selectedLineHeight : lineHeight;
  }
#ifdef BITMAP16_STEAM_DECK
  canvas.setTextSize(1);
  canvas.setTextColor(theme.textSecondary);
  canvas.setTextAlign(TextAlign::Left);
  canvas.drawString("LB/RB:Sections", 3, canvas.height() - 11);
  canvas.setTextAlign(TextAlign::Right);
  canvas.drawString("B:Back", canvas.width() - 3, canvas.height() - 11);
#endif
}

}  // namespace HelpView
}  // namespace bitmap16
