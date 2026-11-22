#pragma once

#include "core/canvas_view.h"

namespace bitmap16::Desktop {
enum class MouseTool { Draw, Erase, Fill };

// PC-only controls follow the same logical coordinates used to render the UI.
inline int colorAtPointer(int width, int height, uint8_t gridSize,
                          uint8_t paletteSize, int x, int y) {
  const auto layout = CanvasView::layoutFor(width, height, gridSize);
  const int columns = paletteSize > 8 ? 2 : 1;
#ifdef BITMAP16_PC
  const int left = layout.paletteX;
#else
  const int left = width - columns * layout.paletteSwatchSize - 5;
#endif
  if (x < left || x >= left + columns * layout.paletteSwatchSize ||
      y < layout.gridY || y >= layout.gridY + 8 * layout.paletteSwatchSize) return 0;
  const int color = ((x - left) / layout.paletteSwatchSize) * 8 +
      (y - layout.gridY) / layout.paletteSwatchSize + 1;
  return color <= paletteSize ? color : 0;
}

inline bool toolAtPointer(int width, int height, uint8_t gridSize,
                         int x, int y, MouseTool& tool) {
  const auto layout = CanvasView::layoutFor(width, height, gridSize);
  if (x < layout.toolsX || x >= layout.toolsX + 24) return false;
  const int offset = y - (layout.gridY - 2);
  if (offset < 0 || offset >= 78 || offset % 27 >= 24) return false;
  tool = static_cast<MouseTool>(offset / 27);
  return true;
}
}  // namespace bitmap16::Desktop
