#pragma once

#include <cstdint>

#include "core/sketch.h"
#include "core/sketch_history.h"

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
  bool canUndo() const { return history_.canUndo(); }
  bool canRedo() const { return history_.canRedo(); }

  void setCursor(uint8_t x, uint8_t y);
  bool moveCursor(int dx, int dy);
  void setSelectedColor(uint8_t color);

  bool draw();
  bool erase();
  bool paintTo(uint8_t x, uint8_t y, bool erase, bool connect);
  bool floodFill();
  bool floodFill(uint8_t replacementColor);
  bool clear();
  bool shift(int dx, int dy, bool saveUndoState = true);
  bool toggleGridSize();
  bool applyPalette(const uint16_t* colors, uint8_t paletteSize);
  bool undo();
  bool redo();

  void saveUndo();
  void setHeldActions(bool draw, bool erase, bool move) {
    history_.setHeldActions(draw, erase, move);
  }
  void endUndoGroup() { history_.endGroup(); }
  void endUndoGroup(SketchHistory::Action action) { history_.endGroup(action); }

 private:
  bool isInBounds(int x, int y) const;
  bool containsArtwork() const;

  Sketch sketch_;
  SketchHistory history_;
  uint8_t cursorX_ = 0;
  uint8_t cursorY_ = 0;
  uint8_t selectedColor_ = 1;
};

}  // namespace bitmap16
