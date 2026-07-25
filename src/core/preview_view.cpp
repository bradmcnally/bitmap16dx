#include "core/preview_view.h"

#include <algorithm>

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
    const Theme& theme) {
  if (!canvas.isValid()) {
    return;
  }

  const uint16_t clearColor = backgroundColor(state, theme);
  canvas.fillScreen(clearColor);
  if (image.pixels == nullptr || image.paletteColors == nullptr ||
      (image.gridSize != 8 && image.gridSize != 16)) {
    return;
  }

  constexpr int maximumViewSize = 128;
  const int availableSize = std::min(canvas.width(), canvas.height());
  const int cellSize =
      std::max(1, std::min(maximumViewSize, availableSize) / image.gridSize);
  const int viewSize = cellSize * image.gridSize;
  const int viewX = (canvas.width() - viewSize) / 2;
  const int viewY = (canvas.height() - viewSize) / 2;

  for (int y = 0; y < image.gridSize; ++y) {
    for (int x = 0; x < image.gridSize; ++x) {
      const uint8_t paletteIndex = image.pixels[y][x];
      if (paletteIndex == 0 || paletteIndex > image.paletteSize) {
        continue;
      }
      canvas.fillRect(
          viewX + x * cellSize,
          viewY + y * cellSize,
          cellSize,
          cellSize,
          image.paletteColors[paletteIndex - 1]);
    }
  }
}

}  // namespace PreviewView
}  // namespace bitmap16
