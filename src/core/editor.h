#pragma once

#include <cstdint>

#include "core/sketch.h"

namespace bitmap16 {

class Editor {
 public:
  Editor();

  void reset(const Sketch& sketch);

  const Sketch& sketch() const { return sketch_; }
  Sketch& sketch() { return sketch_; }

  uint8_t cursorX() const { return cursorX_; }
  uint8_t cursorY() const { return cursorY_; }
  uint8_t selectedColor() const { return selectedColor_; }
  bool canUndo() const { return undoAvailable_; }
  bool canRedo() const { return redoAvailable_; }

  void setCursor(uint8_t x, uint8_t y);
  bool moveCursor(int dx, int dy);
  void setSelectedColor(uint8_t color);

  bool draw();
  bool erase();
  bool floodFill();
  bool floodFill(uint8_t replacementColor);
  bool clear();
  bool shift(int dx, int dy, bool saveUndoState = true);
  bool toggleGridSize();
  bool applyPalette(const uint16_t* colors, uint8_t paletteSize);
  bool undo();
  bool redo();

  void saveUndo();

 private:
  bool isInBounds(int x, int y) const;
  bool containsArtwork() const;

  Sketch sketch_;
  Sketch undoSketch_;
  Sketch redoSketch_;
  uint8_t cursorX_ = 0;
  uint8_t cursorY_ = 0;
  uint8_t selectedColor_ = 1;
  bool undoAvailable_ = false;
  bool redoAvailable_ = false;
};

}  // namespace bitmap16
