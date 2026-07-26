#include "steam_screenshot.h"

#include <algorithm>
#include <cstddef>

#include "core/palette.h"

namespace bitmap16 {
namespace Desktop {
namespace SteamScreenshot {
namespace {

uint8_t expand5(uint16_t value) {
  const uint8_t channel = static_cast<uint8_t>(value & 0x1fu);
  return static_cast<uint8_t>((channel << 3) | (channel >> 2));
}

uint8_t expand6(uint16_t value) {
  const uint8_t channel = static_cast<uint8_t>(value & 0x3fu);
  return static_cast<uint8_t>((channel << 2) | (channel >> 4));
}

void writeColor(std::vector<uint8_t>& rgb, std::size_t offset, uint16_t color) {
  rgb[offset] = expand5(color >> 11);
  rgb[offset + 1] = expand6(color >> 5);
  rgb[offset + 2] = expand5(color);
}

}  // namespace

bool render(const Sketch& sketch, uint16_t background, Image& image) {
  if (!isSupportedGridSize(sketch.gridSize) ||
      !isSupportedPaletteSize(sketch.paletteSize)) {
    return false;
  }

  Image rendered;
  rendered.width = kWidth;
  rendered.height = kHeight;
  rendered.rgb.resize(
      static_cast<std::size_t>(rendered.width) * rendered.height * 3u);
  for (std::size_t offset = 0; offset < rendered.rgb.size(); offset += 3) {
    writeColor(rendered.rgb, offset, background);
  }

  const int scale =
      std::max(1, std::min(kWidth, kHeight) / sketch.gridSize);
  const int artworkSize = scale * sketch.gridSize;
  const int originX = (kWidth - artworkSize) / 2;
  const int originY = (kHeight - artworkSize) / 2;
  for (int sourceY = 0; sourceY < sketch.gridSize; ++sourceY) {
    for (int sourceX = 0; sourceX < sketch.gridSize; ++sourceX) {
      const uint8_t index = sketch.pixels[sourceY][sourceX];
      if (index == 0) continue;
      const uint16_t color = Palette::colorForIndex(
          sketch.paletteColors, sketch.paletteSize, index);
      for (int pixelY = 0; pixelY < scale; ++pixelY) {
        for (int pixelX = 0; pixelX < scale; ++pixelX) {
          const std::size_t offset =
              (static_cast<std::size_t>(originY + sourceY * scale + pixelY) *
                   kWidth +
               originX + sourceX * scale + pixelX) *
              3u;
          writeColor(rendered.rgb, offset, color);
        }
      }
    }
  }

  image = std::move(rendered);
  return true;
}

}  // namespace SteamScreenshot
}  // namespace Desktop
}  // namespace bitmap16
