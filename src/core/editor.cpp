#include "core/editor.h"

#include <algorithm>

namespace bitmap16 {

namespace {

struct Point {
  uint8_t x;
  uint8_t y;
};

}  // namespace

Editor::Editor() = default;

void Editor::reset(const Sketch& sketch) {
  sketch_ = sketch;
  if (!isSupportedGridSize(sketch_.gridSize)) {
    sketch_.gridSize = 8;
  }
  if (!isSupportedPaletteSize(sketch_.paletteSize)) {
    sketch_.paletteSize = 16;
  }
  cursorX_ = 0;
  cursorY_ = 0;
  selectedColor_ = 1;
  undoAvailable_ = false;
}

void Editor::setCursor(uint8_t x, uint8_t y) {
  const uint8_t max = static_cast<uint8_t>(sketch_.gridSize - 1);
  cursorX_ = std::min(x, max);
  cursorY_ = std::min(y, max);
}

bool Editor::moveCursor(int dx, int dy) {
  const int nextX = static_cast<int>(cursorX_) + dx;
  const int nextY = static_cast<int>(cursorY_) + dy;
  if (!isInBounds(nextX, nextY)) {
    return false;
  }
  cursorX_ = static_cast<uint8_t>(nextX);
  cursorY_ = static_cast<uint8_t>(nextY);
  return true;
}

void Editor::setSelectedColor(uint8_t color) {
  if (color >= 1 && color <= sketch_.paletteSize) {
    selectedColor_ = color;
  }
}

bool Editor::draw() {
  if (sketch_.pixels[cursorY_][cursorX_] == selectedColor_) {
    return false;
  }
  saveUndo();
  sketch_.pixels[cursorY_][cursorX_] = selectedColor_;
  sketch_.isEmpty = false;
  return true;
}

bool Editor::erase() {
  if (sketch_.pixels[cursorY_][cursorX_] == 0) {
    return false;
  }
  saveUndo();
  sketch_.pixels[cursorY_][cursorX_] = 0;
  sketch_.isEmpty = !containsArtwork();
  return true;
}

bool Editor::floodFill() {
  const uint8_t originalColor = sketch_.pixels[cursorY_][cursorX_];
  if (originalColor == selectedColor_) {
    return false;
  }

  saveUndo();

  bool visited[kMaxGridSize][kMaxGridSize] = {};
  Point stack[kMaxGridSize * kMaxGridSize];
  std::size_t stackSize = 0;
  stack[stackSize++] = {cursorX_, cursorY_};
  visited[cursorY_][cursorX_] = true;

  while (stackSize > 0) {
    const Point point = stack[--stackSize];
    if (sketch_.pixels[point.y][point.x] != originalColor) {
      continue;
    }

    sketch_.pixels[point.y][point.x] = selectedColor_;

    if (point.y > 0 && !visited[point.y - 1][point.x]) {
      stack[stackSize++] = {point.x, static_cast<uint8_t>(point.y - 1)};
      visited[point.y - 1][point.x] = true;
    }
    if (point.y + 1 < sketch_.gridSize && !visited[point.y + 1][point.x]) {
      stack[stackSize++] = {point.x, static_cast<uint8_t>(point.y + 1)};
      visited[point.y + 1][point.x] = true;
    }
    if (point.x > 0 && !visited[point.y][point.x - 1]) {
      stack[stackSize++] = {static_cast<uint8_t>(point.x - 1), point.y};
      visited[point.y][point.x - 1] = true;
    }
    if (point.x + 1 < sketch_.gridSize && !visited[point.y][point.x + 1]) {
      stack[stackSize++] = {static_cast<uint8_t>(point.x + 1), point.y};
      visited[point.y][point.x + 1] = true;
    }
  }

  sketch_.isEmpty = false;
  return true;
}

bool Editor::clear() {
  bool changed = false;
  for (uint8_t y = 0; y < sketch_.gridSize && !changed; ++y) {
    for (uint8_t x = 0; x < sketch_.gridSize; ++x) {
      if (sketch_.pixels[y][x] != 0) {
        changed = true;
        break;
      }
    }
  }
  if (!changed) {
    return false;
  }

  saveUndo();
  for (uint8_t y = 0; y < sketch_.gridSize; ++y) {
    for (uint8_t x = 0; x < sketch_.gridSize; ++x) {
      sketch_.pixels[y][x] = 0;
    }
  }
  sketch_.isEmpty = !containsArtwork();
  return true;
}

bool Editor::shift(int dx, int dy) {
  if (dx == 0 && dy == 0) {
    return false;
  }

  saveUndo();
  uint8_t shifted[kMaxGridSize][kMaxGridSize] = {};
  const int size = sketch_.gridSize;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const int sourceX = (x - dx % size + size) % size;
      const int sourceY = (y - dy % size + size) % size;
      shifted[y][x] = sketch_.pixels[sourceY][sourceX];
    }
  }
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      sketch_.pixels[y][x] = shifted[y][x];
    }
  }
  return true;
}

bool Editor::toggleGridSize() {
  saveUndo();
  sketch_.gridSize = sketch_.gridSize == 8 ? 16 : 8;
  setCursor(cursorX_, cursorY_);
  return true;
}

bool Editor::undo() {
  if (!undoAvailable_) {
    return false;
  }
  sketch_ = undoSketch_;
  setCursor(cursorX_, cursorY_);
  if (selectedColor_ > sketch_.paletteSize) {
    selectedColor_ = 1;
  }
  undoAvailable_ = false;
  return true;
}

void Editor::saveUndo() {
  undoSketch_ = sketch_;
  undoAvailable_ = true;
}

bool Editor::isInBounds(int x, int y) const {
  return x >= 0 && y >= 0 && x < sketch_.gridSize && y < sketch_.gridSize;
}

bool Editor::containsArtwork() const {
  for (std::size_t y = 0; y < kMaxGridSize; ++y) {
    for (std::size_t x = 0; x < kMaxGridSize; ++x) {
      if (sketch_.pixels[y][x] != 0) {
        return true;
      }
    }
  }
  return false;
}

}  // namespace bitmap16
