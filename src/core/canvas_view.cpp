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

uint16_t neutralizeCursorColor(uint16_t color) {
  const int red = ((color >> 11) & 0x1f) * 255 / 31;
  const int green = ((color >> 5) & 0x3f) * 255 / 63;
  const int blue = (color & 0x1f) * 255 / 31;
  const int gray = (red * 30 + green * 59 + blue * 11) / 100;
  // Retain most of the underlying hue while removing the warm cast inherited
  // from the light-theme checkerboard.
  constexpr int kColorWeight = 3;
  constexpr int kTotalWeight = 4;
  const int neutralRed =
      (red * kColorWeight + gray) / kTotalWeight;
  const int neutralGreen =
      (green * kColorWeight + gray) / kTotalWeight;
  const int neutralBlue =
      (blue * kColorWeight + gray) / kTotalWeight;
  return static_cast<uint16_t>(
      ((neutralRed * 31 / 255) << 11) |
      ((neutralGreen * 63 / 255) << 5) |
      (neutralBlue * 31 / 255));
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

void cutPanelCorners(
    Canvas& canvas,
    int x,
    int y,
    int width,
    int height,
    const Theme& theme) {
  canvas.fillRect(x, y, 2, 2, theme.background);
  canvas.fillRect(x + width - 2, y, 2, 2, theme.background);
  canvas.fillRect(x, y + height - 2, 2, 2, theme.background);
  canvas.fillRect(
      x + width - 2,
      y + height - 2,
      2,
      2,
      theme.shadow);
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
  const int pressedOffsetY = pressed ? 1 : 0;
  for (int row = 0; row < icon.height; ++row) {
    for (int column = 0; column < icon.width; ++column) {
      const uint8_t value = iconValue(column, row);
      if (value == 1) {
        canvas.drawPixel(
            x + column, y + pressedOffsetY + row, theme.iconDark);
      } else if (value == 2) {
        canvas.drawPixel(
            x + column, y + pressedOffsetY + row, theme.iconLight);
      }
    }
  }
}

void cutGridCorners(
    Canvas& canvas, const Layout& layout, const Theme& theme) {
  cutPanelCorners(
      canvas,
      layout.gridX,
      layout.gridY,
      layout.gridPixels,
      layout.gridPixels,
      theme);
}

}  // namespace

Layout layoutFor(int width, int height, uint8_t gridSize) {
  constexpr int kPaletteRailGap = 19;
  constexpr int kToolRailGap = 17;
  constexpr int kToolIconWidth = 24;
  const int logicalSize =
      isSupportedGridSize(gridSize) ? gridSize : 8;
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

bool keepCursorVisible(
    Viewport& viewport,
    int width,
    int height,
    uint8_t gridSize,
    uint8_t cursorX,
    uint8_t cursorY) {
  if (!isSupportedGridSize(gridSize)) return false;
  const Layout layout = layoutFor(width, height, gridSize);
  const int cellSize = viewport.cellSize == 0
      ? layout.cellSize
      : std::max(layout.cellSize, static_cast<int>(viewport.cellSize));
  const int visible = std::max(1, layout.gridPixels / cellSize);
  const int maximumOffset = std::max(0, gridSize - visible);
  int nextX = std::min<int>(viewport.x, maximumOffset);
  int nextY = std::min<int>(viewport.y, maximumOffset);
  if (cursorX < nextX) nextX = cursorX;
  if (cursorX >= nextX + visible) nextX = cursorX - visible + 1;
  if (cursorY < nextY) nextY = cursorY;
  if (cursorY >= nextY + visible) nextY = cursorY - visible + 1;
  nextX = std::max(0, std::min(maximumOffset, nextX));
  nextY = std::max(0, std::min(maximumOffset, nextY));
  const bool changed =
      nextX != viewport.x || nextY != viewport.y;
  viewport.x = static_cast<uint8_t>(nextX);
  viewport.y = static_cast<uint8_t>(nextY);
  return changed;
}

bool adjustZoom(
    Viewport& viewport,
    int delta,
    int width,
    int height,
    uint8_t gridSize,
    uint8_t cursorX,
    uint8_t cursorY) {
  if (delta == 0 || !isSupportedGridSize(gridSize)) return false;
  const Layout layout = layoutFor(width, height, gridSize);
  const int current = viewport.cellSize == 0
      ? layout.cellSize
      : viewport.cellSize;
  const int maximum = std::max(layout.cellSize, 16);
  const int next = delta > 0
      ? std::min(maximum, current * 2)
      : std::max(layout.cellSize, current / 2);
  if (next == current) return false;
  viewport.cellSize =
      next == layout.cellSize ? 0 : static_cast<uint8_t>(next);
  const int visible = std::max(1, layout.gridPixels / next);
  viewport.x = static_cast<uint8_t>(std::max(
      0,
      std::min<int>(
          gridSize - visible,
          static_cast<int>(cursorX) - visible / 2)));
  viewport.y = static_cast<uint8_t>(std::max(
      0,
      std::min<int>(
          gridSize - visible,
          static_cast<int>(cursorY) - visible / 2)));
  return true;
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

  Layout layout =
      layoutFor(canvas.width(), canvas.height(), state.gridSize);
  if (state.toolsAtLeftEdge) {
    layout.toolsX = 3;
    layout.statusX = 3;
  }
  const int logicalSize =
      isSupportedGridSize(state.gridSize) ? state.gridSize : 8;
  const int cellSize = state.viewportCellSize == 0
      ? layout.cellSize
      : std::max(
            layout.cellSize,
            static_cast<int>(state.viewportCellSize));
  const int visibleCells =
      std::max(1, layout.gridPixels / cellSize);
  const int viewportX = std::max(
      0,
      std::min(
          logicalSize - visibleCells,
          static_cast<int>(state.viewportX)));
  const int viewportY = std::max(
      0,
      std::min(
          logicalSize - visibleCells,
          static_cast<int>(state.viewportY)));
  const int viewportRight =
      std::min(logicalSize, viewportX + visibleCells);
  const int viewportBottom =
      std::min(logicalSize, viewportY + visibleCells);
  drawShadow(
      canvas,
      layout.gridX,
      layout.gridY,
      layout.gridPixels,
      layout.gridPixels,
      theme);
  for (int y = viewportY; y < viewportBottom; ++y) {
    for (int x = viewportX; x < viewportRight; ++x) {
      const uint8_t pixel = state.pixels[y][x];
      const int cellX =
          layout.gridX + (x - viewportX) * cellSize;
      const int cellY =
          layout.gridY + (y - viewportY) * cellSize;
      if (pixel != 0) {
        const uint16_t color = Palette::colorForIndex(
            state.paletteColors, state.paletteSize, pixel);
        canvas.fillRect(
            cellX, cellY, cellSize, cellSize, color);
      } else {
        const int checkSize = std::max(1, cellSize / 2);
        for (int py = 0; py < cellSize; py += checkSize) {
          for (int px = 0; px < cellSize; px += checkSize) {
            const bool dark =
                (((cellX + px) / checkSize) +
                 ((cellY + py) / checkSize)) % 2 == 0;
            const uint16_t color =
                dark ? theme.cellDark : theme.cellLight;
            canvas.fillRect(
                cellX + px,
                cellY + py,
                std::min(checkSize, cellSize - px),
                std::min(checkSize, cellSize - py),
                color);
          }
        }
      }
    }
  }

  // Rulers use the same neutral darkening as the cursor and sit above both
  // painted artwork and the empty-cell checkerboard.
  if (state.rulersVisible) {
    // The ruler belongs to the visible canvas frame, not the full document.
    // Keeping it at the viewport center means it remains visible while a
    // zoomed document pans underneath it.
    const int centerX =
        layout.gridX + layout.gridPixels / 2;
    const int centerY =
        layout.gridY + layout.gridPixels / 2;
    for (int pixelY = layout.gridY;
         pixelY < layout.gridY + layout.gridPixels;
         ++pixelY) {
      for (int pixelX = layout.gridX;
           pixelX < layout.gridX + layout.gridPixels;
           ++pixelX) {
        if (pixelX == centerX || pixelY == centerY) {
          canvas.drawPixel(
              pixelX,
              pixelY,
              neutralizeCursorColor(
                  scaleColor(canvas.readPixel(pixelX, pixelY), 0.8f)));
        }
      }
    }
  }

  // Apply the cursor last so a ruler passing through the focused cell remains
  // beneath the cursor treatment.
  if (!state.moveMode &&
      state.cursorX >= viewportX && state.cursorX < viewportRight &&
      state.cursorY >= viewportY && state.cursorY < viewportBottom) {
    const int selectedCellX =
        layout.gridX + (state.cursorX - viewportX) * cellSize;
    const int selectedCellY =
        layout.gridY + (state.cursorY - viewportY) * cellSize;
    const bool selectedEmpty =
        state.pixels[state.cursorY][state.cursorX] == 0;
    for (int py = 0; py < cellSize; ++py) {
      for (int px = 0; px < cellSize; ++px) {
        // Use one transform across an empty dark-theme cell. Alternating the
        // strength per checker tile compounds the underlying checkerboard and
        // creates a distracting secondary pattern.
        const float shade = selectedEmpty && theme.dark ? 0.5f : 0.8f;
        const int pixelX = selectedCellX + px;
        const int pixelY = selectedCellY + py;
        canvas.drawPixel(
            pixelX,
            pixelY,
            neutralizeCursorColor(
                scaleColor(canvas.readPixel(pixelX, pixelY), shade)));
      }
    }
  }

  cutGridCorners(canvas, layout, theme);

  const int columns = state.paletteSize > 8 ? 2 : 1;
#ifdef BITMAP16_STEAM_DECK
  const int paletteStartX = layout.paletteX;
#else
  const int paletteStartX =
      canvas.width() - columns * layout.paletteSwatchSize - 5;
#endif
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
      layout.gridX + (state.cursorX - viewportX) * cellSize;
  const int cursorCellY =
      layout.gridY + (state.cursorY - viewportY) * cellSize;
  if (assets != nullptr) {
    const int toolsY = layout.gridY - 2;
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
    const bool zoomed = cellSize > layout.cellSize;
    if (state.batteryPercent >= 0 && !zoomed) {
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

    if (zoomed) {
      // Only 16x16 and 32x32 canvases can enter the zoomed state. A fixed
      // 32px panel renders them at integer 2x and 1x scales respectively.
      constexpr int minimapSize = 32;
      const int minimapScale = minimapSize / logicalSize;
      const int minimapX = layout.toolsX;
      const int minimapY = toolsY + 82;
      drawShadow(
          canvas,
          minimapX,
          minimapY,
          minimapSize,
          minimapSize,
          theme);
      canvas.fillRect(
          minimapX,
          minimapY,
          minimapSize,
          minimapSize,
          theme.cellDark);
      for (int sourceY = 0; sourceY < logicalSize; ++sourceY) {
        for (int sourceX = 0; sourceX < logicalSize; ++sourceX) {
          const uint8_t index = state.pixels[sourceY][sourceX];
          if (index > 0) {
            canvas.fillRect(
                minimapX + sourceX * minimapScale,
                minimapY + sourceY * minimapScale,
                minimapScale,
                minimapScale,
                Palette::colorForIndex(
                    state.paletteColors, state.paletteSize, index));
          }
        }
      }
      cutPanelCorners(
          canvas,
          minimapX,
          minimapY,
          minimapSize,
          minimapSize,
          theme);
      const int keyX =
          minimapX + viewportX * minimapSize / logicalSize;
      const int keyY =
          minimapY + viewportY * minimapSize / logicalSize;
      const int keyWidth = std::max(
          2, visibleCells * minimapSize / logicalSize);
      const int keyHeight = std::max(
          2, visibleCells * minimapSize / logicalSize);
      canvas.drawRect(
          keyX, keyY, keyWidth, keyHeight, theme.iconDark);
      if (keyWidth > 2 && keyHeight > 2) {
        canvas.drawRect(
            keyX + 1,
            keyY + 1,
            keyWidth - 2,
            keyHeight - 2,
            theme.iconLight);
      }
    }

    const Icon& cursor =
        state.moveMode ? assets->moveCursor : assets->cursor;
    drawIndexedIcon(
        canvas,
        cursorCellX + cellSize +
            (state.moveMode
                 ? assets->moveCursorOffsetX
                 : assets->cursorOffsetX),
        cursorCellY + cellSize +
            (state.moveMode
                 ? assets->moveCursorOffsetY
                 : assets->cursorOffsetY),
        cursor,
        theme);
  } else {
    canvas.drawRect(
        cursorCellX,
        cursorCellY,
        cellSize,
        cellSize,
        theme.text);
  }

  canvas.setTextColor(theme.text);
  if (state.status != nullptr && state.status[0] != '\0') {
    canvas.setTextAlign(
        state.statusCentered ? TextAlign::Center : TextAlign::Left);
    canvas.setTextSize(1);
    canvas.drawString(
        state.status,
        state.statusCentered ? canvas.width() / 2 : layout.statusX,
        canvas.height() - 11);
  }
  if (assets == nullptr && state.batteryPercent >= 0) {
    // Large enough for every possible int value, a percent sign, and NUL.
    // GCC's -Wformat-truncation checks the type's full range rather than the
    // platform contract that battery percentages are limited to 0..100.
    char battery[16];
    std::snprintf(battery, sizeof(battery), "%d%%", state.batteryPercent);
    canvas.setTextAlign(TextAlign::Center);
    canvas.setTextSize(1);
    canvas.drawString(battery, 14, 91);
  }
}

}  // namespace CanvasView
}  // namespace bitmap16
