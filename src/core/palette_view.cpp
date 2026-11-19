#include "core/palette_view.h"
#include "core/palette.h"

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

void drawStatus(Canvas& canvas, const char* text, uint16_t color) {
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  int x = 3;
  const int y = canvas.height() - 11;
  for (int index = 0; text[index] != '\0'; ++index) {
    if (text[index] == '|') {
      int characters = 0;
      for (int right = index + 1; text[right] != '\0'; ++right) {
        if (text[right] == '[' && text[right + 1] != '\0' && text[right + 2] == ']') right += 2;
        ++characters;
      }
      x = canvas.width() - 3 - characters * 6;
      continue;
    }
    const bool underline = text[index] == '[' && text[index + 1] != '\0' &&
        text[index + 2] == ']';
    const char label[] = {text[index + (underline ? 1 : 0)], '\0'};
    canvas.drawString(label, x, y);
    if (underline) {
      canvas.drawFastHLine(x, y + 8, 5, color);
      index += 2;
    }
    x += 6;
  }
}

constexpr float kEntranceCartridgeMs = 480.0f;
constexpr float kEntranceStaggerMs = 90.0f;
constexpr float kExitCartridgeMs = 240.0f;
constexpr float kExitStaggerMs = 65.0f;
constexpr float kReloadFadeOutMs = 100.0f;
constexpr float kReloadFadeInMs = 220.0f;
constexpr float kReloadStaggerMs = 75.0f;

float smoothStep(float value) {
  value = std::max(0.0f, std::min(1.0f, value));
  return value * value * (3.0f - 2.0f * value);
}

uint16_t blendColor(uint16_t color, uint16_t background, int alpha) {
  const int inverse = 256 - alpha;
  const int red = (((color >> 11) * alpha + (background >> 11) * inverse) >> 8);
  const int green = ((((color >> 5) & 63) * alpha + ((background >> 5) & 63) * inverse) >> 8);
  const int blue = (((color & 31) * alpha + (background & 31) * inverse) >> 8);
  return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
}

void fadeCartridge(Canvas& canvas, int x, int y, int alpha, uint16_t background) {
  if (alpha >= 256) return;
  const int left = std::max(0, x);
  const int right = std::min(canvas.width(), x + kCartridgeWidth);
  const int top = std::max(0, y);
  const int bottom = std::min(canvas.height(), y + kCartridgeHeight);
  for (int row = top; row < bottom; ++row) {
    uint16_t* pixels = canvas.pixels() + row * canvas.width();
    for (int column = left; column < right; ++column)
      pixels[column] = blendColor(pixels[column], background, alpha);
  }
}

void compositeReload(Canvas& canvas, const State& state, const Theme& theme) {
  if (!state.reloadAnimating || state.reloadFrame == nullptr) return;
  const int centerX = canvas.width() / 2;
  for (int x = 0; x < canvas.width(); ++x) {
    const int slot = static_cast<int>(std::lround(
        static_cast<float>(x - centerX) / (kCartridgeWidth + kPaletteGap)));
    const float elapsed = state.reloadElapsedMs - std::abs(slot) * kReloadStaggerMs;
    const int oldAlpha = static_cast<int>(256 * (1.0f - smoothStep(elapsed / kReloadFadeOutMs)));
    const int newAlpha = static_cast<int>(256 * smoothStep(
        (elapsed - kReloadFadeOutMs) / kReloadFadeInMs));
    for (int y = 18; y < canvas.height() - 13; ++y) {
      const int pixel = y * canvas.width() + x;
      const uint16_t old = blendColor(state.reloadFrame[pixel], theme.background, oldAlpha);
      canvas.pixels()[pixel] = blendColor(canvas.pixels()[pixel], old, newAlpha);
    }
  }
}

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
#if defined(BITMAP16_CARDPUTER_ZERO) || defined(BITMAP16_STEAM_DECK) || defined(BITMAP16_LOSPEC)
  if (!theme.dark) {
    canvas.pushImage(x, y, kCartridgeWidth, kCartridgeHeight, graphic);
    return;
  }
  // The source art is immutable. Prepare its dark-theme colors once, then
  // copy clipped rows rather than remapping every pixel on every frame.
  static uint16_t darkGraphic[kCartridgeWidth * kCartridgeHeight];
  static const uint16_t* cachedSource = nullptr;
  static uint16_t cachedBackground = 0;
  if (cachedSource != graphic || cachedBackground != theme.background) {
    for (int row = 0; row < kCartridgeHeight; ++row) {
      for (int column = 0; column < kCartridgeWidth; ++column) {
        darkGraphic[row * kCartridgeWidth + column] = cartridgeColor(
            graphic[row * kCartridgeWidth + column], theme, column, row);
      }
    }
    cachedSource = graphic;
    cachedBackground = theme.background;
  }
  canvas.pushImage(x, y, kCartridgeWidth, kCartridgeHeight, darkGraphic);
#else
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
#endif
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
  const int columns = (entry.size + 3) / 4;
  int index = 0;
  for (int column = 0; column < columns; ++column) {
    const int rows = entry.size / columns + (column < entry.size % columns ? 1 : 0);
    const int left = column * 64 / columns;
    const int right = (column + 1) * 64 / columns;
    for (int row = 0; row < rows; ++row) {
      const int top = row * 64 / rows;
      const int bottom = (row + 1) * 64 / rows;
      drawCutRect(canvas, x + left, y + top, right - left, bottom - top,
                  entry.colors[index++], column == 0 && row == 0,
                  column == columns - 1 && row == 0,
                  column == 0 && row == rows - 1,
                  column == columns - 1 && row == rows - 1);
    }
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
      if (!Palette::matchesSizeFilter(entry.size, state.filterSize)) {
        continue;
      }
      if (state.userOnly && !entry.user) {
        continue;
      }
      state.filteredIndices[state.filteredCount++] =
          static_cast<CatalogIndex>(index);
    }
  }
  if (state.filteredCount == 0 || state.cursor >= state.filteredCount) {
    state.cursor = 0;
    state.scrollPosition = 0.0f;
  }
}

bool moveCursor(State& state, int delta) {
  if (state.filteredCount <= 0 || state.insertionAnimating || state.exitAnimating) {
    return false;
  }
  const int next = std::max(
      0, std::min(state.filteredCount - 1, state.cursor + delta));
  if (next == state.cursor) {
    return false;
  }
  state.entranceAnimating = false;
  state.reloadAnimating = false;
  state.reloadFrame = nullptr;
  state.cursor = next;
  return true;
}

uint8_t steppedSizeFilter(uint8_t current, int direction) {
  constexpr uint8_t filters[] = {0, 4, 8, 16};
  int index = 0;
  for (int i = 0; i < 4; ++i) if (filters[i] == current) index = i;
  return filters[(index + (direction < 0 ? 3 : direction > 0 ? 1 : 0)) % 4];
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

void beginEntrance(State& state, int viewportWidth) {
  state.reloadAnimating = false;
  state.reloadFrame = nullptr;
  if (state.filteredCount <= 0 || state.insertionAnimating || state.exitAnimating) return;
  state.entranceAnimating = true;
  state.entranceElapsedMs = 0.0f;
  state.entranceCenter = state.cursor;
  state.scrollPosition = static_cast<float>(state.cursor);
  const int visibleRadius = (viewportWidth / 2 + kCartridgeWidth / 2 - 1) /
      (kCartridgeWidth + kPaletteGap);
  state.entranceDurationMs = kEntranceCartridgeMs + visibleRadius * kEntranceStaggerMs;
}

void beginReload(State& state, int viewportWidth, const uint16_t* previousFrame) {
  state.entranceAnimating = false;
  state.exitAnimating = false;
  state.reloadAnimating = previousFrame != nullptr;
  state.reloadFrame = previousFrame;
  state.reloadElapsedMs = 0.0f;
  state.scrollPosition = static_cast<float>(state.cursor);
  const int visibleRadius = (viewportWidth / 2 + kCartridgeWidth / 2 - 1) /
      (kCartridgeWidth + kPaletteGap);
  state.reloadDurationMs = kReloadFadeOutMs + kReloadFadeInMs +
      visibleRadius * kReloadStaggerMs;
}

void beginExit(State& state, int viewportWidth) {
  if (state.insertionAnimating || state.exitAnimating) return;
  state.entranceAnimating = false;
  state.exitAnimating = true;
  state.exitElapsedMs = 0.0f;
  state.exitCenter = static_cast<int>(std::lround(state.scrollPosition));
  const int visibleRadius = (viewportWidth / 2 + kCartridgeWidth / 2 - 1) /
      (kCartridgeWidth + kPaletteGap);
  state.exitDurationMs = state.filteredCount == 0 ? 0.0f :
      kExitCartridgeMs + visibleRadius * kExitStaggerMs;
}

bool beginSelection(State& state) {
  if (selectedCatalogIndex(state) < 0 || state.insertionAnimating || state.exitAnimating) {
    return false;
  }
  state.entranceAnimating = false;
  state.reloadAnimating = false;
  state.reloadFrame = nullptr;
  state.frozenScrollPosition = state.scrollPosition;
  state.insertionAnimating = true;
  state.insertionProgress = 0.0f;
  return true;
}

AnimationResult advance(
    State& state,
    float scrollSpeed,
    float insertionSpeed) {
  if (state.reloadAnimating) {
    state.reloadElapsedMs = std::min(state.reloadDurationMs,
        state.reloadElapsedMs + insertionSpeed * kInsertionDurationMs);
    if (state.reloadElapsedMs >= state.reloadDurationMs) {
      state.reloadAnimating = false;
      state.reloadFrame = nullptr;
    }
    return AnimationResult::Animating;
  }
  if (state.exitAnimating) {
    state.exitElapsedMs = std::min(state.exitDurationMs,
        state.exitElapsedMs + insertionSpeed * kInsertionDurationMs);
    return state.exitElapsedMs >= state.exitDurationMs
        ? AnimationResult::ExitComplete : AnimationResult::Animating;
  }
  if (state.entranceAnimating) {
    state.entranceElapsedMs += insertionSpeed * kInsertionDurationMs;
    if (state.entranceElapsedMs >= state.entranceDurationMs) {
      state.entranceAnimating = false;
      state.entranceElapsedMs = state.entranceDurationMs;
    }
    return AnimationResult::Animating;
  }
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
    return difference == 0.0f ? AnimationResult::Idle : AnimationResult::Animating;
  }
  state.scrollPosition += difference * scrollSpeed;
  return AnimationResult::Animating;
}

AnimationResult advanceTimed(State& state, float elapsedMs) {
  if (elapsedMs <= 0.0f) return AnimationResult::Idle;
  // Match the original 25% easing at 16 ms, independent of render speed.
  const float scrollSpeed = 1.0f - std::pow(0.75f, elapsedMs / 16.0f);
  return advance(state, scrollSpeed, elapsedMs / kInsertionDurationMs);
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
  canvas.drawString(catalog.title, 4, 4);

  char filter[20] = {};
  if (state.filterSize != 0 && state.userOnly) {
    std::snprintf(filter, sizeof(filter), "USER %s",
                  state.filterSize == 4 ? "4" : state.filterSize == 8 ? "5-8" : "9-16");
  } else if (state.filterSize != 0) {
    std::snprintf(filter, sizeof(filter), "%s COLORS",
                  state.filterSize == 4 ? "4" : state.filterSize == 8 ? "5-8" : "9-16");
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
        catalog.emptyMessage, canvas.width() / 2, canvas.height() / 2);
    compositeReload(canvas, state, theme);
    if (statusMessage != nullptr) {
      canvas.setTextAlign(TextAlign::Left);
      drawStatus(canvas, statusMessage, theme.text);
    }
    return;
  }

  const int centerX = canvas.width() / 2;
  const int centerY = std::max(66, canvas.height() / 2 - 1);
  const float scroll = state.insertionAnimating
      ? state.frozenScrollPosition
      : state.scrollPosition;
  const int scrollPixels = static_cast<int>(std::lround(
      scroll * (kCartridgeWidth + kPaletteGap)));
  for (int filtered = 0; filtered < state.filteredCount; ++filtered) {
    const int catalogIndex = state.filteredIndices[filtered];
    if (catalogIndex < 0 || catalogIndex >= catalog.count) {
      continue;
    }
    // Round the carousel's shared offset once. Rounding each cartridge's
    // signed displacement separately lets one neighbor move after landing.
    const int paletteX = centerX +
        filtered * (kCartridgeWidth + kPaletteGap) - scrollPixels;
    if (paletteX <= -kCartridgeWidth / 2 ||
        paletteX >= canvas.width() + kCartridgeWidth / 2) {
      continue;
    }
    const bool selected = filtered == state.cursor;
    int cartridgeY = centerY - kCartridgeHeight / 2;
    const float focus = selected && state.insertionAnimating ? 1.0f :
        std::max(0.0f, 1.0f - std::fabs(static_cast<float>(filtered) - scroll));
    cartridgeY -= static_cast<int>(std::lround(8.0f * focus));
    int entranceAlpha = 256;
    if (state.reloadAnimating) {
      const float elapsed = state.reloadElapsedMs -
          std::abs(filtered - state.cursor) * kReloadStaggerMs - kReloadFadeOutMs;
      cartridgeY -= static_cast<int>(std::lround(8.0f *
          (1.0f - smoothStep(elapsed / kReloadFadeInMs))));
    }
    if (state.entranceAnimating) {
      const float localMs = state.entranceElapsedMs -
          std::abs(filtered - state.entranceCenter) * kEntranceStaggerMs;
      if (localMs <= 0.0f) continue;
      const float progress = std::min(1.0f, localMs / kEntranceCartridgeMs);
      entranceAlpha = static_cast<int>(256.0f * smoothStep(localMs / 180.0f));
      const int insertionDistance = std::max(0,
          canvas.height() - kCartridgeHeight + kInsertionOvershoot - cartridgeY);
      const float rise = progress < 0.45f
          ? smoothStep(progress / 0.45f)
          : 1.0f - smoothStep((progress - 0.45f) / 0.55f);
      cartridgeY -= static_cast<int>(std::lround(insertionDistance * 0.32f * rise));
    }
    if (state.exitAnimating) {
      const float localMs = state.exitElapsedMs -
          std::abs(filtered - state.exitCenter) * kExitStaggerMs;
      const float progress = smoothStep(localMs / kExitCartridgeMs);
      entranceAlpha = static_cast<int>(256.0f * (1.0f - progress));
      if (entranceAlpha <= 0) continue;
      const int insertionDistance = std::max(0,
          canvas.height() - kCartridgeHeight + kInsertionOvershoot - cartridgeY);
      cartridgeY -= static_cast<int>(std::lround(insertionDistance * 0.32f * progress));
    }
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
          "%s%s%.30s (%u)",
          catalogIndex == activeCatalogIndex ? "> " : "",
          entry.user ? "* " : "",
          entry.name == nullptr ? "UNTITLED" : entry.name, entry.size);
      canvas.setTextAlign(TextAlign::Center);
      canvas.setTextColor(blendColor(theme.text, theme.background, entranceAlpha));
      canvas.drawString(
          label, centerX, centerY + kCartridgeHeight / 2 + 6);
      if ((entry.likes >= 0 || (entry.author != nullptr && entry.author[0] != '\0')) &&
          canvas.height() >= 170) {
        char details[48] = {};
        if (entry.likes >= 0) std::snprintf(details, sizeof(details), "%.28s  %d likes",
                      entry.author == nullptr || entry.author[0] == '\0'
                          ? "Lospec" : entry.author,
                      entry.likes);
        else std::snprintf(details, sizeof(details), "by %.40s", entry.author);
        canvas.setTextColor(blendColor(theme.textSecondary, theme.background, entranceAlpha));
        canvas.drawString(details, centerX, centerY + kCartridgeHeight / 2 + 18);
      }
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
        (entry.size >= 4 && entry.size <= 16)) {
      drawSwatches(
          canvas,
          paletteX - kCartridgeWidth / 2 + 8,
          cartridgeY + 6,
          entry);
    }
    fadeCartridge(canvas, paletteX - kCartridgeWidth / 2, cartridgeY,
                  entranceAlpha, theme.background);
  }

  compositeReload(canvas, state, theme);
  if (statusMessage != nullptr && statusMessage[0] != '\0') {
    canvas.setTextAlign(TextAlign::Left);
    canvas.setTextSize(1);
    canvas.setTextColor(theme.text);
    drawStatus(canvas, statusMessage, theme.text);
  }
}

}  // namespace PaletteView
}  // namespace bitmap16
