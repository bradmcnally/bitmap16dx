#pragma once

#include <cstdint>

#include "core/canvas.h"
#include "core/sketch.h"

namespace bitmap16 {
namespace CanvasView {

struct Viewport {
  uint8_t cellSize = 0;
  uint8_t x = 0;
  uint8_t y = 0;
};

struct State {
  const uint8_t (*pixels)[kMaxGridSize] = nullptr;
  uint8_t gridSize = 8;
  const uint16_t* paletteColors = nullptr;
  uint8_t paletteSize = 16;
  uint8_t cursorX = 0;
  uint8_t cursorY = 0;
  uint8_t selectedColor = 1;
  bool rulersVisible = false;
  bool moveMode = false;
  const char* status = nullptr;
  int batteryPercent = -1;
  bool drawPressed = false;
  bool erasePressed = false;
  bool fillPressed = false;
  uint8_t viewportCellSize = 0;
  uint8_t viewportX = 0;
  uint8_t viewportY = 0;
  bool statusCentered = false;
};

struct Theme {
  uint16_t background;
  uint16_t cellDark;
  uint16_t cellLight;
  uint16_t shadow;
  uint16_t text;
  uint16_t textSecondary;
  uint16_t centerLine;
  uint16_t iconDark;
  uint16_t iconLight;
  bool dark;
};

struct Icon {
  const uint8_t* pixels = nullptr;
  int width = 0;
  int height = 0;
};

struct Assets {
  Icon draw;
  Icon erase;
  Icon fill;
  Icon battery[4];
  Icon cursor;
  Icon moveCursor;
  int cursorOffsetX = 0;
  int cursorOffsetY = 0;
  int moveCursorOffsetX = 0;
  int moveCursorOffsetY = 0;
};

struct Layout {
  int gridX;
  int gridY;
  int gridPixels;
  int cellSize;
  int toolsX;
  int paletteX;
  int paletteSwatchSize;
  int statusX;
};

Layout layoutFor(int width, int height, uint8_t gridSize);
bool adjustZoom(
    Viewport& viewport,
    int delta,
    int width,
    int height,
    uint8_t gridSize,
    uint8_t cursorX,
    uint8_t cursorY);
bool keepCursorVisible(
    Viewport& viewport,
    int width,
    int height,
    uint8_t gridSize,
    uint8_t cursorX,
    uint8_t cursorY);
void render(
    Canvas& canvas,
    const State& state,
    const Theme& theme,
    const Assets* assets = nullptr);

}  // namespace CanvasView
}  // namespace bitmap16
