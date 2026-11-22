#pragma once
#include <algorithm>
#include <cmath>

namespace bitmap16::Desktop {
struct DisplayLayout {
  int x, y, width, height, scale;
  int logicalX(int windowX, int windowWidth, int outputWidth) const {
    return static_cast<int>(std::floor((windowX * (double)outputWidth / windowWidth - x) / scale));
  }
  int logicalY(int windowY, int windowHeight, int outputHeight) const {
    return static_cast<int>(std::floor((windowY * (double)outputHeight / windowHeight - y) / scale));
  }
};
inline DisplayLayout displayLayout(int outputWidth, int outputHeight,
    int windowWidth, int windowHeight, int logicalWidth, int logicalHeight) {
  const double density = std::min((double)outputWidth / std::max(1, windowWidth),
                                 (double)outputHeight / std::max(1, windowHeight));
  // Cap at 4x in screen points, while retaining crisp output on Retina displays.
  const int scale = std::max(1, std::min({outputWidth / logicalWidth,
      outputHeight / logicalHeight, static_cast<int>(4 * density)}));
  // Keep the UI pixel size capped, but let the drawable view reach the window
  // edges rather than clipping it to a centered, fixed-size framebuffer.
  const int width = outputWidth - outputWidth % scale;
  const int height = outputHeight - outputHeight % scale;
  return {(outputWidth - width) / 2, (outputHeight - height) / 2, width, height, scale};
}
}  // namespace bitmap16::Desktop
