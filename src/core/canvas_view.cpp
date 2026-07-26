#include "core/canvas_view.h"

#include <algorithm>
#include <cstdio>

#include "core/palette.h"

namespace bitmap16 {
namespace CanvasView {
namespace {

uint16_t scaleColor(uint16_t color, float scale) {
  return static_cast<uint16_t>(
      (static_cast<uint16_t>(((color >> 11) & 0x1f) * scale) << 11) |
      (static_cast<uint16_t>(((color >> 5) & 0x3f) * scale) << 5) |
      static_cast<uint16_t>((color & 0x1f) * scale));
}

void drawShadow(
    Canvas& canvas,
    int x,
    int y,
    int width,
    int height,
    const Theme& theme) {
  canvas.fillRect(x + 2, y + 2, width, height, theme.shadow);
  canvas.fillRect(x + width, y + 2, 2, 2, theme.background);
  canvas.fillRect(x + 2, y + height, 2, 2, theme.background);
  canvas.fillRect(x + width, y + height, 2, 2, theme.background);
}

void drawIndexedIcon(
    Canvas& canvas,
    int x,
    int y,
    const Icon& icon,
    const Theme& theme,
    bool pressed = false) {
  if (icon.pixels == nullptr) return;
  const auto iconValue = [&](int column, int row) {
    const int pixel = row * icon.width + column;
    const int shift = (3 - pixel % 4) * 2;
    return static_cast<uint8_t>(
        (icon.pixels[pixel / 4] >> shift) & 0x03);
  };
  const uint16_t dark =
      pressed ? scaleColor(theme.iconDark, 0.8f) : theme.iconDark;
  const uint16_t light =
      pressed ? scaleColor(theme.iconLight, 0.8f) : theme.iconLight;
  const int pressedOffsetY = pressed ? 1 : 0;
  for (int row = 0; row < icon.height; ++row) {
    for (int column = 0; column < icon.width; ++column) {
      const uint8_t value = iconValue(column, row);
      if (value == 1) {
        canvas.drawPixel(x + column, y + pressedOffsetY + row, dark);
      } else if (value == 2) {
        canvas.drawPixel(x + column, y + pressedOffsetY + row, light);
      }
    }
  }
}

void cutGridCorners(
    Canvas& canvas, const Layout& layout, const Theme& theme) {
  canvas.fillRect(layout.gridX, layout.gridY, 2, 2, theme.background);
  canvas.fillRect(
      layout.gridX + layout.gridPixels - 2,
      layout.gridY,
      2,
      2,
      theme.background);
  canvas.fillRect(
      layout.gridX,
      layout.gridY + layout.gridPixels - 2,
      2,
      2,
      theme.background);
  canvas.fillRect(
      layout.gridX + layout.gridPixels - 2,
      layout.gridY + layout.gridPixels - 2,
      2,
      2,
      theme.shadow);
}

}  // namespace

Layout layoutFor(int width, int height, uint8_t gridSize) {
  constexpr int kPaletteRailGap = 19;
  constexpr int kToolRailGap = 17;
  constexpr int kToolIconWidth = 24;
  const int logicalSize = gridSize == 16 ? 16 : 8;
  const int available = std::max(8, std::min(128, height - 7));
  const int cellSize = std::max(1, available / logicalSize);
  const int gridPixels = cellSize * logicalSize;
  const int paletteSwatchSize =
      std::max(8, std::min(16, height / 8));
  const int gridX = (width - gridPixels) / 2;
  const int toolsX =
      std::max(3, gridX - kToolRailGap - kToolIconWidth);
  const int paletteX = std::min(
      width - paletteSwatchSize * 2 - 5,
      gridX + gridPixels + kPaletteRailGap);
  return {
      gridX,
      (height - gridPixels + 1) / 2,
      gridPixels,
      cellSize,
      toolsX,
      paletteX,
      paletteSwatchSize,
      toolsX,
  };
}

void render(
    Canvas& canvas,
    const State& state,
    const Theme& theme,
    const Assets* assets) {
  canvas.fillScreen(theme.background);
  if (state.pixels == nullptr || state.paletteColors == nullptr) {
    return;
  }

  const Layout layout =
      layoutFor(canvas.width(), canvas.height(), state.gridSize);
  const int logicalSize = state.gridSize == 16 ? 16 : 8;
  drawShadow(
      canvas,
      layout.gridX,
      layout.gridY,
      layout.gridPixels,
      layout.gridPixels,
      theme);
  for (int y = 0; y < logicalSize; ++y) {
    for (int x = 0; x < logicalSize; ++x) {
      const uint8_t pixel = state.pixels[y][x];
      const int cellX = layout.gridX + x * layout.cellSize;
      const int cellY = layout.gridY + y * layout.cellSize;
      const bool selected = x == state.cursorX && y == state.cursorY;
      if (pixel != 0) {
        uint16_t color = Palette::colorForIndex(
            state.paletteColors, state.paletteSize, pixel);
        if (selected) color = scaleColor(color, 0.8f);
        canvas.fillRect(
            cellX, cellY, layout.cellSize, layout.cellSize, color);
      } else {
        const int checkSize = std::max(1, layout.cellSize / 2);
        for (int py = 0; py < layout.cellSize; py += checkSize) {
          for (int px = 0; px < layout.cellSize; px += checkSize) {
            const bool dark =
                (((cellX + px) / checkSize) +
                 ((cellY + py) / checkSize)) % 2 == 0;
            uint16_t color = dark ? theme.cellDark : theme.cellLight;
            if (selected) {
              color = scaleColor(
                  color, theme.dark ? (dark ? 0.4f : 0.2f) : 0.8f);
            }
            canvas.fillRect(
                cellX + px,
                cellY + py,
                std::min(checkSize, layout.cellSize - px),
                std::min(checkSize, layout.cellSize - py),
                color);
          }
        }
        if (state.rulersVisible) {
          const int centerX = layout.gridX + layout.gridPixels / 2;
          const int centerY = layout.gridY + layout.gridPixels / 2;
          if (centerX >= cellX &&
              centerX < cellX + layout.cellSize) {
            canvas.drawFastVLine(
                centerX, cellY, layout.cellSize, theme.centerLine);
          }
          if (centerY >= cellY &&
              centerY < cellY + layout.cellSize) {
            canvas.drawFastHLine(
                cellX, centerY, layout.cellSize, theme.centerLine);
          }
        }
      }
    }
  }

  cutGridCorners(canvas, layout, theme);

  const int columns = state.paletteSize > 8 ? 2 : 1;
  const int paletteStartX = layout.paletteX;
  const int paletteHeight =
      std::min<int>(8, state.paletteSize) * layout.paletteSwatchSize;
  drawShadow(
      canvas,
      paletteStartX,
      layout.gridY,
      columns * layout.paletteSwatchSize,
      paletteHeight,
      theme);
  for (int index = 0; index < state.paletteSize; ++index) {
    const int column = index / 8;
    const int row = index % 8;
    const int x = paletteStartX + column * layout.paletteSwatchSize;
    const int y = layout.gridY + row * layout.paletteSwatchSize;
    canvas.fillRect(
        x,
        y,
        layout.paletteSwatchSize,
        layout.paletteSwatchSize,
        state.paletteColors[index]);
    const bool selected = index + 1 == state.selectedColor;
    if (!selected) {
      if (index == 0) {
        canvas.fillRect(x, y, 2, 2, theme.background);
        if (columns == 1) {
          canvas.fillRect(
              x + layout.paletteSwatchSize - 2,
              y,
              2,
              2,
              theme.background);
        }
      }
      if (index == state.paletteSize - 1) {
        canvas.fillRect(
            x + (columns == 1 ? 0 : layout.paletteSwatchSize - 2),
            y + layout.paletteSwatchSize - 2,
            2,
            2,
            columns == 1 ? theme.background : theme.shadow);
        canvas.fillRect(
            x + layout.paletteSwatchSize - 2,
            y + layout.paletteSwatchSize - 2,
            2,
            2,
            theme.shadow);
      }
      if (columns == 2 && index == 7) {
        canvas.fillRect(
            x, y + layout.paletteSwatchSize - 2, 2, 2,
            theme.background);
      } else if (columns == 2 && index == 8) {
        canvas.fillRect(
            x + layout.paletteSwatchSize - 2, y, 2, 2,
            theme.background);
      }
    }
  }

  const int selectedIndex =
      std::max(0, std::min<int>(state.paletteSize - 1,
                               state.selectedColor - 1));
  const int selectedColumn = columns == 1 ? 0 : selectedIndex / 8;
  const int selectedRow = selectedIndex % 8;
  const int selectedX =
      paletteStartX + selectedColumn * layout.paletteSwatchSize;
  const int selectedY =
      layout.gridY + selectedRow * layout.paletteSwatchSize;
  canvas.fillRect(
      selectedX - 2, selectedY - 2,
      layout.paletteSwatchSize + 4, 2, 0x0000);
  canvas.fillRect(
      selectedX - 2, selectedY + layout.paletteSwatchSize,
      layout.paletteSwatchSize + 4, 2, 0x0000);
  canvas.fillRect(
      selectedX - 2, selectedY - 2,
      2, layout.paletteSwatchSize + 4, 0x0000);
  canvas.fillRect(
      selectedX + layout.paletteSwatchSize, selectedY - 2,
      2, layout.paletteSwatchSize + 4, 0x0000);
  canvas.drawRect(
      selectedX,
      selectedY,
      layout.paletteSwatchSize,
      layout.paletteSwatchSize,
      theme.iconLight);
  if (layout.paletteSwatchSize > 3) {
    canvas.drawRect(
        selectedX + 1,
        selectedY + 1,
        layout.paletteSwatchSize - 2,
        layout.paletteSwatchSize - 2,
        theme.iconLight);
  }

  const int cursorCellX =
      layout.gridX + state.cursorX * layout.cellSize;
  const int cursorCellY =
      layout.gridY + state.cursorY * layout.cellSize;
  if (assets != nullptr) {
    const int toolsY = layout.gridY - 2;
    if (!state.hideToolIcons) {
      drawIndexedIcon(
          canvas,
          layout.toolsX,
          toolsY,
          assets->draw,
          theme,
          state.drawPressed);
      drawIndexedIcon(
          canvas,
          layout.toolsX,
          toolsY + 27,
          assets->erase,
          theme,
          state.erasePressed);
      drawIndexedIcon(
          canvas,
          layout.toolsX,
          toolsY + 54,
          assets->fill,
          theme,
          state.fillPressed);
    }
    if (!state.hideToolIcons && state.batteryPercent >= 0) {
      int batteryStage = 0;
      if (state.batteryPercent >= 90) batteryStage = 3;
      else if (state.batteryPercent >= 50) batteryStage = 2;
      else if (state.batteryPercent >= 10) batteryStage = 1;
      drawIndexedIcon(
          canvas,
          layout.toolsX,
          toolsY + 82,
          assets->battery[batteryStage],
          theme);
    }

    const Icon& cursor =
        state.moveMode ? assets->moveCursor : assets->cursor;
    drawIndexedIcon(
        canvas,
        cursorCellX + layout.cellSize +
            (state.moveMode
                 ? assets->moveCursorOffsetX
                 : assets->cursorOffsetX),
        cursorCellY + layout.cellSize +
            (state.moveMode
                 ? assets->moveCursorOffsetY
                 : assets->cursorOffsetY),
        cursor,
        theme);
  } else {
    canvas.drawRect(
        cursorCellX,
        cursorCellY,
        layout.cellSize,
        layout.cellSize,
        theme.text);
  }

  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextColor(theme.text);
  if (state.status != nullptr && state.status[0] != '\0') {
    canvas.drawString(
        state.status, layout.statusX, canvas.height() - 11);
  }
  if (assets == nullptr && state.batteryPercent >= 0) {
    // Large enough for every possible int value, a percent sign, and NUL.
    // GCC's -Wformat-truncation checks the type's full range rather than the
    // platform contract that battery percentages are limited to 0..100.
    char battery[16];
    std::snprintf(battery, sizeof(battery), "%d%%", state.batteryPercent);
    canvas.setTextAlign(TextAlign::Center);
    canvas.drawString(battery, 14, 91);
  }
}

}  // namespace CanvasView
}  // namespace bitmap16
