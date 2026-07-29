#include "core/palette_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace bitmap16 {
namespace PaletteView {

namespace {

constexpr int kCartridgeWidth = 80;
constexpr int kCartridgeHeight = 92;
constexpr int kPaletteGap = 20;
constexpr int kInsertionOvershoot = 13;
constexpr float kInsertionImpactAt = 0.72f;

float insertionEasing(float progress) {
  constexpr float kWindUpEnd = 0.23f;
  constexpr float kHoldEnd = 0.58f;
  constexpr float kWindUpDistance = -0.32f;
  constexpr float kImpactOvershoot = 1.025f;
  const float clamped = std::max(0.0f, std::min(1.0f, progress));

  const auto easeInOutCirc = [](float value) {
    if (value < 0.5f) {
      const float doubled = 2.0f * value;
      return (1.0f - std::sqrt(
          std::max(0.0f, 1.0f - doubled * doubled))) / 2.0f;
    }
    const float inverse = -2.0f * value + 2.0f;
    return (std::sqrt(
        std::max(0.0f, 1.0f - inverse * inverse)) + 1.0f) / 2.0f;
  };

  // Beat 1: a controlled pull-back that eases gently at both ends.
  if (clamped < kWindUpEnd) {
    return kWindUpDistance *
        easeInOutCirc(clamped / kWindUpEnd);
  }

  // Beat 2: hold just long enough for the anticipation to register.
  if (clamped < kHoldEnd) {
    return kWindUpDistance;
  }

  // Beat 3: accelerate hard into the slot.
  if (clamped < kInsertionImpactAt) {
    const float phase =
        (clamped - kHoldEnd) /
        (kInsertionImpactAt - kHoldEnd);
    // Quadratic acceleration starts traveling sooner than the previous cubic
    // curve while still gaining speed all the way to impact.
    const float plunge = phase * phase;
    return kWindUpDistance +
        (kImpactOvershoot - kWindUpDistance) * plunge;
  }

  // A tiny settle keeps the landing crisp rather than bouncy.
  const float phase =
      (clamped - kInsertionImpactAt) /
      (1.0f - kInsertionImpactAt);
  const float easeOut =
      1.0f - (1.0f - phase) * (1.0f - phase);
  return kImpactOvershoot +
      (1.0f - kImpactOvershoot) * easeOut;
}

void drawImpactSparks(
    Canvas& canvas,
    int centerX,
    int slotY,
    const Entry& entry,
    const Theme& theme,
    float insertionProgress) {
  if (insertionProgress < kInsertionImpactAt ||
      insertionProgress >= 1.0f) {
    return;
  }

  const float phase =
      (insertionProgress - kInsertionImpactAt) /
      (1.0f - kInsertionImpactAt);
  const float travel = phase * (2.0f - phase);
  constexpr int horizontal[] = {-26, -20, -14, -8, 8, 14, 20, 26};
  constexpr int vertical[] = {-10, -22, -30, -18, -18, -30, -22, -10};
  // Emit from both cartridge edges so particles rendered behind it remain
  // visible instead of being fully occluded by its 80px body.
  constexpr int startX[] = {-38, -36, -34, -32, 32, 34, 36, 38};

  for (int index = 0; index < 8; ++index) {
    // Outer sparks disappear first, producing a crisp stepped breakup rather
    // than an alpha fade.
    const float lifetime = 0.72f + 0.04f * (index % 4);
    if (phase >= lifetime) {
      continue;
    }
    const int x =
        centerX + startX[index] +
        static_cast<int>(horizontal[index] * travel);
    const int y =
        slotY +
        static_cast<int>(vertical[index] * travel) +
        static_cast<int>(7.0f * phase * phase);
    uint16_t color = theme.text;
    if (entry.colors != nullptr && entry.size > 0 && index % 4 != 0) {
      color = entry.colors[index % entry.size];
    }
    canvas.fillRect(x, y, 2, 2, color);
  }
}

uint16_t cartridgeColor(
    uint16_t color,
    const Theme& theme,
    int column,
    int row) {
  if (!theme.dark) {
    return color;
  }
  if (color == 0xd69b) {
    // The downward registration mark shares the source image's background-key
    // color, but it is printed cartridge artwork and should not change with
    // the application theme.
    const bool registrationMark =
        column >= 35 && column <= 44 &&
        row >= 74 && row <= 79;
    if (registrationMark) {
      return color;
    }
    return theme.background;
  }
  if (color == 0xc63a) {
    return 0x0000;
  }
  return color;
}

void drawCartridge(
    Canvas& canvas,
    int x,
    int y,
    const Theme& theme,
    const uint16_t* graphic) {
  if (graphic == nullptr) {
    canvas.fillRect(x + 2, y + 2, kCartridgeWidth, kCartridgeHeight, 0x0000);
    canvas.fillRect(
        x, y, kCartridgeWidth, kCartridgeHeight, theme.textSecondary);
    canvas.fillRect(x + 6, y + 4, 68, 68, theme.background);
    return;
  }
  for (int row = 0; row < kCartridgeHeight; ++row) {
    for (int column = 0; column < kCartridgeWidth; ++column) {
      canvas.drawPixel(
          x + column,
          y + row,
          cartridgeColor(
              graphic[row * kCartridgeWidth + column],
              theme,
              column,
              row));
    }
  }
}

void drawCutRect(
    Canvas& canvas,
    int x,
    int y,
    int width,
    int height,
    uint16_t color,
    bool cutTopLeft,
    bool cutTopRight,
    bool cutBottomLeft,
    bool cutBottomRight) {
  constexpr int cut = 2;
  canvas.fillRect(
      x + (cutTopLeft ? cut : 0),
      y,
      width - (cutTopLeft ? cut : 0) - (cutTopRight ? cut : 0),
      cut,
      color);
  canvas.fillRect(x, y + cut, width, height - cut * 2, color);
  canvas.fillRect(
      x + (cutBottomLeft ? cut : 0),
      y + height - cut,
      width - (cutBottomLeft ? cut : 0) -
          (cutBottomRight ? cut : 0),
      cut,
      color);
}

void drawSwatches(
    Canvas& canvas,
    int x,
    int y,
    const Entry& entry) {
  const int columns = entry.size == 4 ? 1 : entry.size == 8 ? 2 : 4;
  const int rows = 4;
  const int colorWidth = 64 / columns;
  const int colorHeight = 64 / rows;
  for (int index = 0; index < entry.size; ++index) {
    const int column = entry.size == 4 ? 0 : index / rows;
    const int row = entry.size == 4 ? index : index % rows;
    drawCutRect(
        canvas,
        x + column * colorWidth,
        y + row * colorHeight,
        colorWidth,
        colorHeight,
        entry.colors[index],
        column == 0 && row == 0,
        column == columns - 1 && row == 0,
        column == 0 && row == rows - 1,
        column == columns - 1 && row == rows - 1);
  }
}

}  // namespace

void reset(State& state, const Catalog& catalog) {
  state = {};
  rebuildFilter(state, catalog);
}

void rebuildFilter(State& state, const Catalog& catalog) {
  state.filteredCount = 0;
  if (catalog.entries != nullptr) {
    const int count = std::max(0, std::min(kMaximumEntries, catalog.count));
    for (int index = 0; index < count; ++index) {
      const Entry& entry = catalog.entries[index];
      if (state.filterSize != 0 && entry.size != state.filterSize) {
        continue;
      }
      if (state.userOnly && !entry.user) {
        continue;
      }
      state.filteredIndices[state.filteredCount++] =
          static_cast<uint8_t>(index);
    }
  }
  if (state.filteredCount == 0 || state.cursor >= state.filteredCount) {
    state.cursor = 0;
    state.scrollPosition = 0.0f;
  }
}

bool moveCursor(State& state, int delta) {
  if (state.filteredCount <= 0 || state.insertionAnimating) {
    return false;
  }
  const int next = std::max(
      0, std::min(state.filteredCount - 1, state.cursor + delta));
  if (next == state.cursor) {
    return false;
  }
  state.cursor = next;
  return true;
}

bool toggleSizeFilter(
    State& state,
    const Catalog& catalog,
    uint8_t size) {
  if (size != 4 && size != 8 && size != 16) {
    return false;
  }
  state.filterSize = state.filterSize == size ? 0 : size;
  rebuildFilter(state, catalog);
  return true;
}

bool toggleUserFilter(State& state, const Catalog& catalog) {
  state.userOnly = !state.userOnly;
  rebuildFilter(state, catalog);
  return true;
}

int selectedCatalogIndex(const State& state) {
  if (state.filteredCount <= 0 ||
      state.cursor < 0 || state.cursor >= state.filteredCount) {
    return -1;
  }
  return state.filteredIndices[state.cursor];
}

void selectCatalogIndex(State& state, int catalogIndex) {
  for (int index = 0; index < state.filteredCount; ++index) {
    if (state.filteredIndices[index] == catalogIndex) {
      state.cursor = index;
      state.scrollPosition = static_cast<float>(index);
      return;
    }
  }
}

bool beginSelection(State& state) {
  if (selectedCatalogIndex(state) < 0 || state.insertionAnimating) {
    return false;
  }
  state.frozenScrollPosition = state.scrollPosition;
  state.insertionAnimating = true;
  state.insertionProgress = 0.0f;
  return true;
}

AnimationResult advance(
    State& state,
    float scrollSpeed,
    float insertionSpeed) {
  if (state.insertionAnimating) {
    state.insertionProgress += insertionSpeed;
    if (state.insertionProgress >= 1.0f) {
      state.insertionProgress = 1.0f;
      return AnimationResult::SelectionComplete;
    }
    return AnimationResult::Animating;
  }
  const float difference =
      static_cast<float>(state.cursor) - state.scrollPosition;
  if (std::fabs(difference) <= 0.01f) {
    state.scrollPosition = static_cast<float>(state.cursor);
    return AnimationResult::Idle;
  }
  state.scrollPosition += difference * scrollSpeed;
  return AnimationResult::Animating;
}

void render(
    Canvas& canvas,
    const State& state,
    const Catalog& catalog,
    int activeCatalogIndex,
    const Theme& theme,
    const uint16_t* cartridgeGraphic,
    const char* statusMessage) {
  if (!canvas.isValid()) {
    return;
  }
  canvas.fillScreen(theme.background);
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(theme.text);
  canvas.drawString("PALETTES", 4, 4);

  char filter[20] = {};
  if (state.filterSize != 0 && state.userOnly) {
    std::snprintf(filter, sizeof(filter), "USER+%u", state.filterSize);
  } else if (state.filterSize != 0) {
    std::snprintf(filter, sizeof(filter), "%u-COLOR", state.filterSize);
  } else if (state.userOnly) {
    std::snprintf(filter, sizeof(filter), "USER");
  }
  if (filter[0] != '\0') {
    canvas.setTextAlign(TextAlign::Right);
    canvas.drawString(filter, canvas.width() - 4, 4);
  }

  if (state.filteredCount <= 0 || catalog.entries == nullptr) {
    canvas.setTextAlign(TextAlign::Center);
    canvas.setTextColor(theme.textSecondary);
    canvas.drawString(
        "NO PALETTES", canvas.width() / 2, canvas.height() / 2);
    return;
  }

  const int centerX = canvas.width() / 2;
  const int centerY = std::max(66, canvas.height() / 2 - 1);
  const float scroll = state.insertionAnimating
      ? state.frozenScrollPosition
      : state.scrollPosition;
  for (int filtered = 0; filtered < state.filteredCount; ++filtered) {
    const int catalogIndex = state.filteredIndices[filtered];
    if (catalogIndex < 0 || catalogIndex >= catalog.count) {
      continue;
    }
    const int paletteX = centerX + static_cast<int>(
        (static_cast<float>(filtered) - scroll) *
        (kCartridgeWidth + kPaletteGap));
    if (paletteX <= -kCartridgeWidth / 2 ||
        paletteX >= canvas.width() + kCartridgeWidth / 2) {
      continue;
    }
    const bool selected = filtered == state.cursor;
    int cartridgeY = centerY - kCartridgeHeight / 2;
    if (selected && state.insertionAnimating) {
      const float easedProgress = insertionEasing(state.insertionProgress);
      const int insertionDistance = std::max(
          0,
          canvas.height() - kCartridgeHeight +
              kInsertionOvershoot - cartridgeY);
      cartridgeY += static_cast<int>(
          insertionDistance * easedProgress);
    }

    const Entry& entry = catalog.entries[catalogIndex];
    if (selected && !state.insertionAnimating) {
      char label[48] = {};
      std::snprintf(
          label,
          sizeof(label),
          "%s%s%s",
          catalogIndex == activeCatalogIndex ? "> " : "",
          entry.user ? "* " : "",
          entry.name == nullptr ? "UNTITLED" : entry.name);
      canvas.setTextAlign(TextAlign::Center);
      canvas.setTextColor(theme.text);
      canvas.drawString(
          label, centerX, centerY + kCartridgeHeight / 2 + 6);
    }
    if (selected && state.insertionAnimating) {
      drawImpactSparks(
          canvas,
          paletteX,
          canvas.height() - 10,
          entry,
          theme,
          state.insertionProgress);
    }
    drawCartridge(
        canvas,
        paletteX - kCartridgeWidth / 2,
        cartridgeY,
        theme,
        cartridgeGraphic);
    if (entry.colors != nullptr &&
        (entry.size == 4 || entry.size == 8 || entry.size == 16)) {
      drawSwatches(
          canvas,
          paletteX - kCartridgeWidth / 2 + 8,
          cartridgeY + 6,
          entry);
    }
  }

  if (statusMessage != nullptr && statusMessage[0] != '\0') {
    canvas.setTextAlign(TextAlign::Left);
    canvas.setTextSize(1);
    canvas.setTextColor(theme.text);
    canvas.drawString(statusMessage, 3, canvas.height() - 11);
  }
}

}  // namespace PaletteView
}  // namespace bitmap16
