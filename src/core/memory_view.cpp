#include "core/memory_view.h"

#include <algorithm>
#include <cmath>

namespace bitmap16 {
namespace MemoryView {

namespace {

constexpr int kThumbnailSize = 32;
constexpr int kGap = 8;
constexpr int kTitleHeight = 14;
constexpr int kTopMargin = 5;
constexpr int kBottomMargin = 5;

void drawCreate(Canvas& canvas, int x, int y, const Theme& theme) {
  constexpr int cutSize = 2;
  constexpr int dashLength = 4;
  constexpr int gapLength = 4;
  for (int i = cutSize; i < kThumbnailSize - cutSize;
       i += dashLength + gapLength) {
    const int length =
        std::min(dashLength, kThumbnailSize - cutSize - i);
    canvas.fillRect(x + i, y, length, 2, theme.thumbnail);
    canvas.fillRect(
        x + i, y + kThumbnailSize - 2, length, 2, theme.thumbnail);
    canvas.fillRect(x, y + i, 2, length, theme.thumbnail);
    canvas.fillRect(
        x + kThumbnailSize - 2, y + i, 2, length, theme.thumbnail);
  }
  const int centerX = x + kThumbnailSize / 2;
  const int centerY = y + kThumbnailSize / 2;
  constexpr int plusSize = 15;
  constexpr int plusThickness = 3;
  canvas.fillRect(
      centerX - plusThickness / 2,
      centerY - plusSize / 2,
      plusThickness,
      plusSize,
      theme.text);
  canvas.fillRect(
      centerX - plusSize / 2,
      centerY - plusThickness / 2,
      plusSize,
      plusThickness,
      theme.text);
}

void drawThumbnail(
    Canvas& canvas,
    int x,
    int y,
    const Entry& entry,
    const Theme& theme) {
  if (entry.pixels != nullptr && entry.paletteColors != nullptr &&
      isSupportedGridSize(entry.gridSize)) {
    const int scale = std::max(1, kThumbnailSize / entry.gridSize);
    const int artworkSize = entry.gridSize * scale;
    const int artworkX = x + (kThumbnailSize - artworkSize) / 2;
    const int artworkY = y + (kThumbnailSize - artworkSize) / 2;
    for (int sourceY = 0; sourceY < entry.gridSize; ++sourceY) {
      for (int sourceX = 0; sourceX < entry.gridSize; ++sourceX) {
        const uint8_t index = entry.pixels[sourceY][sourceX];
        if (index == 0 || index > entry.paletteSize) {
          continue;
        }
        canvas.fillRect(
            artworkX + sourceX * scale,
            artworkY + sourceY * scale,
            scale,
            scale,
            entry.paletteColors[index - 1]);
      }
    }
  }
  canvas.fillRect(x, y, 2, 2, theme.background);
  canvas.fillRect(x + kThumbnailSize - 2, y, 2, 2, theme.background);
  canvas.fillRect(x, y + kThumbnailSize - 2, 2, 2, theme.background);
  canvas.fillRect(
      x + kThumbnailSize - 2,
      y + kThumbnailSize - 2,
      2,
      2,
      theme.background);
  if (entry.active) {
    canvas.drawRect(x - 1, y - 1, kThumbnailSize + 2, kThumbnailSize + 2, theme.active);
  }
}

void drawCursor(
    Canvas& canvas,
    int x,
    int y,
    float phase,
    const Theme& theme,
    const Assets* assets) {
  constexpr float kTau = 6.28318530718f;
  constexpr int cutSize = 2;
  canvas.fillRect(x, y, cutSize, cutSize, theme.background);
  canvas.fillRect(
      x + kThumbnailSize - cutSize,
      y,
      cutSize,
      cutSize,
      theme.background);
  canvas.fillRect(
      x,
      y + kThumbnailSize - cutSize,
      cutSize,
      cutSize,
      theme.background);
  canvas.fillRect(
      x + kThumbnailSize - cutSize,
      y + kThumbnailSize - cutSize,
      cutSize,
      cutSize,
      theme.background);
  if (assets != nullptr && assets->selectorCorner != nullptr &&
      assets->selectorWidth > 0 && assets->selectorHeight > 0) {
    const float breath = (std::sin(phase * kTau) + 1.0f) * 0.5f;
    const int offset = static_cast<int>(breath * 4.0f + 0.5f);
    constexpr int cornerOffset = 6;
    const auto drawCorner =
        [&](int cornerX, int cornerY, bool flipX, bool flipY) {
          for (int row = 0; row < assets->selectorHeight; ++row) {
            for (int column = 0; column < assets->selectorWidth; ++column) {
              const int pixelIndex =
                  row * assets->selectorWidth + column;
              const int shift = (3 - pixelIndex % 4) * 2;
              const uint8_t value =
                  (assets->selectorCorner[pixelIndex / 4] >> shift) & 0x03;
              if (value == 0) continue;
              const int drawX = flipX
                  ? cornerX + assets->selectorWidth - 1 - column
                  : cornerX + column;
              const int drawY = flipY
                  ? cornerY + assets->selectorHeight - 1 - row
                  : cornerY + row;
              canvas.drawPixel(
                  drawX,
                  drawY,
                  value == 1
                      ? theme.selectionDark
                      : theme.selectionLight);
            }
          }
        };
    drawCorner(
        x - cornerOffset + offset,
        y - cornerOffset + offset,
        false,
        false);
    drawCorner(
        x + kThumbnailSize + cornerOffset -
            assets->selectorWidth - offset,
        y - cornerOffset + offset,
        true,
        false);
    drawCorner(
        x - cornerOffset + offset,
        y + kThumbnailSize + cornerOffset -
            assets->selectorHeight - offset,
        false,
        true);
    drawCorner(
        x + kThumbnailSize + cornerOffset -
            assets->selectorWidth - offset,
        y + kThumbnailSize + cornerOffset -
            assets->selectorHeight - offset,
        true,
        true);
    return;
  }

  const int offset =
      3 + static_cast<int>((std::sin(phase * kTau) + 1.0f) * 1.5f);
  const int left = x - offset;
  const int top = y - offset;
  const int right = x + kThumbnailSize + offset - 1;
  const int bottom = y + kThumbnailSize + offset - 1;
  constexpr int length = 9;
  canvas.drawFastHLine(left, top, length, theme.selectionLight);
  canvas.drawFastVLine(left, top, length, theme.selectionLight);
  canvas.drawFastHLine(
      right - length + 1, top, length, theme.selectionLight);
  canvas.drawFastVLine(right, top, length, theme.selectionLight);
  canvas.drawFastHLine(left, bottom, length, theme.selectionLight);
  canvas.drawFastVLine(
      left, bottom - length + 1, length, theme.selectionLight);
  canvas.drawFastHLine(
      right - length + 1, bottom, length, theme.selectionLight);
  canvas.drawFastVLine(
      right, bottom - length + 1, length, theme.selectionLight);
}

}  // namespace

int columnCount(int width) {
  return std::max(1, std::min(5, (width - 8 + kGap) / (kThumbnailSize + kGap)));
}

void clamp(State& state, int sketchCount) {
  const int maximum = std::max(0, sketchCount);
  state.cursor = std::max(0, std::min(maximum, state.cursor));
}

bool moveCursor(
    State& state,
    int deltaX,
    int deltaY,
    int sketchCount,
    int width) {
  clamp(state, sketchCount);
  const int columns = columnCount(width);
  const int totalItems = std::max(1, sketchCount + 1);
  int next = state.cursor;
  if (deltaY < 0 && next >= columns) {
    next -= columns;
  } else if (deltaY > 0) {
    const int column = next % columns;
    const int nextRow = next + columns;
    if (nextRow >= totalItems) {
      const int lastRow = ((totalItems - 1) / columns) * columns;
      next = std::min(totalItems - 1, lastRow + column);
    } else {
      next = nextRow;
    }
  } else if (deltaX < 0 && next % columns != 0) {
    --next;
  } else if (
      deltaX > 0 && next % columns != columns - 1 &&
      next < totalItems - 1) {
    ++next;
  }
  if (next == state.cursor) {
    return false;
  }
  state.cursor = next;
  return true;
}

bool advance(
    State& state,
    int sketchCount,
    int width,
    int height,
    float deltaSeconds,
    float scrollSpeed) {
  clamp(state, sketchCount);
  const int columns = columnCount(width);
  const int totalItems = std::max(1, sketchCount + 1);
  const int itemHeight = kThumbnailSize + kGap;
  const int cursorRow = state.cursor / columns;
  const int topBound = kTitleHeight + kTopMargin;
  const int bottomBound = height - kBottomMargin - kThumbnailSize;
  const int cursorY =
      topBound + cursorRow * itemHeight - state.scrollOffset;
  if (cursorY > bottomBound) {
    state.scrollOffset += cursorY - bottomBound;
  } else if (cursorY < topBound) {
    state.scrollOffset -= topBound - cursorY;
  }
  const int rows = (totalItems + columns - 1) / columns;
  const int contentHeight =
      kTitleHeight + kTopMargin + rows * kThumbnailSize +
      std::max(0, rows - 1) * kGap;
  state.scrollOffset = std::max(
      0, std::min(std::max(0, contentHeight - (height - kBottomMargin)),
                  state.scrollOffset));

  const float previousScroll = state.scrollPosition;
  const float difference =
      static_cast<float>(state.scrollOffset) - state.scrollPosition;
  if (std::fabs(difference) > 0.5f) {
    state.scrollPosition += difference * scrollSpeed;
  } else {
    state.scrollPosition = static_cast<float>(state.scrollOffset);
  }
  state.cursorAnimationPhase += deltaSeconds * 0.6f;
  while (state.cursorAnimationPhase >= 1.0f) {
    state.cursorAnimationPhase -= 1.0f;
  }
  return std::fabs(previousScroll - state.scrollPosition) > 0.01f ||
      deltaSeconds > 0.0f;
}

void render(
    Canvas& canvas,
    const State& state,
    const Catalog& catalog,
    const Theme& theme,
    const char* statusMessage,
    const Assets* assets) {
  if (!canvas.isValid()) {
    return;
  }
  canvas.fillScreen(theme.background);
  const int columns = columnCount(canvas.width());
  const int totalItems = std::max(1, catalog.count + 1);
  const int totalWidth =
      columns * kThumbnailSize + (columns - 1) * kGap;
  const int startX = (canvas.width() - totalWidth) / 2;
  const int baseY =
      kTitleHeight + kTopMargin - static_cast<int>(state.scrollPosition);
  const int titleY = -static_cast<int>(state.scrollPosition);

  if (titleY > -kTitleHeight && titleY < canvas.height()) {
    canvas.setTextAlign(TextAlign::Left);
    canvas.setTextSize(1);
    canvas.setTextColor(theme.text);
    canvas.drawString("SKETCHES", 4, titleY + 4);
  }

  for (int item = 0; item < totalItems; ++item) {
    const int x = startX + (item % columns) * (kThumbnailSize + kGap);
    const int y = baseY + (item / columns) * (kThumbnailSize + kGap);
    if (y < -kThumbnailSize - 8 || y > canvas.height() + 8) {
      continue;
    }
    if (item == 0) {
      drawCreate(canvas, x, y, theme);
    } else if (catalog.entries != nullptr && item - 1 < catalog.count) {
      drawThumbnail(canvas, x, y, catalog.entries[item - 1], theme);
    }
    if (item == state.cursor) {
      drawCursor(
          canvas, x, y, state.cursorAnimationPhase, theme, assets);
    }
  }

  if (statusMessage != nullptr && statusMessage[0] != '\0') {
    canvas.setTextAlign(TextAlign::Left);
    canvas.setTextSize(1);
    canvas.setTextColor(theme.text);
    canvas.drawString(statusMessage, 3, canvas.height() - 11);
  }
}

}  // namespace MemoryView
}  // namespace bitmap16
