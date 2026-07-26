#include "core/preview_view.h"

#include <algorithm>

#include "core/palette.h"

namespace bitmap16 {
namespace PreviewView {

bool selectBackground(State& state, int selection) {
  if (selection < static_cast<int>(Background::Black) ||
      selection > static_cast<int>(Background::Dark)) {
    return false;
  }
  const Background next = static_cast<Background>(selection);
  if (next == state.background) {
    return false;
  }
  state.background = next;
  return true;
}

int resolvedZoom(
    const State& state,
    uint8_t gridSize,
    int availableSize) {
  if (!isSupportedGridSize(gridSize)) {
    return 1;
  }
  const int maximum = std::max(1, availableSize / gridSize);
  return state.zoom == 0
      ? maximum
      : std::max(1, std::min(maximum, static_cast<int>(state.zoom)));
}

bool adjustZoom(
    State& state,
    int delta,
    uint8_t gridSize,
    int availableSize) {
  if (delta == 0 || !isSupportedGridSize(gridSize)) {
    return false;
  }
  const int current = resolvedZoom(state, gridSize, availableSize);
  const int maximum = std::max(1, availableSize / gridSize);
  const int next = std::max(1, std::min(maximum, current + delta));
  if (next == current) {
    return false;
  }
  state.zoom = static_cast<uint8_t>(next);
  return true;
}

uint16_t backgroundColor(const State& state, const Theme& theme) {
  switch (state.background) {
    case Background::White:
      return theme.white;
    case Background::Gray:
      return theme.gray;
    case Background::Dark:
      return theme.dark;
    case Background::Black:
    default:
      return theme.black;
  }
}

void render(
    Canvas& canvas,
    const State& state,
    const Image& image,
    const Theme& theme,
    const char* statusMessage,
    bool statusCentered) {
  if (!canvas.isValid()) {
    return;
  }

  const uint16_t clearColor = backgroundColor(state, theme);
  canvas.fillScreen(clearColor);
  if (image.pixels == nullptr || image.paletteColors == nullptr ||
      !isSupportedGridSize(image.gridSize)) {
    return;
  }

  const int availableSize = std::min(canvas.width(), canvas.height());
  const int cellSize =
      resolvedZoom(state, image.gridSize, availableSize);
  const int viewSize = cellSize * image.gridSize;
  const int viewX = (canvas.width() - viewSize) / 2;
  const int viewY = (canvas.height() - viewSize + 1) / 2;

  for (int y = 0; y < image.gridSize; ++y) {
    for (int x = 0; x < image.gridSize; ++x) {
      const uint8_t paletteIndex = image.pixels[y][x];
      if (paletteIndex == 0) {
        continue;
      }
      canvas.fillRect(
          viewX + x * cellSize,
          viewY + y * cellSize,
          cellSize,
          cellSize,
          Palette::colorForIndex(
              image.paletteColors, image.paletteSize, paletteIndex));
    }
  }

  if (statusMessage != nullptr && statusMessage[0] != '\0') {
    const bool darkBackground =
        state.background == Background::Black ||
        state.background == Background::Dark;
    canvas.setTextAlign(
        statusCentered ? TextAlign::Center : TextAlign::Left);
    canvas.setTextSize(1);
    canvas.setTextColor(darkBackground ? theme.white : theme.black);
    canvas.drawString(
        statusMessage,
        statusCentered ? canvas.width() / 2 : 3,
        canvas.height() - 11);
  }
}

}  // namespace PreviewView
}  // namespace bitmap16
