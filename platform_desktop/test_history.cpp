#include <cstdlib>
#include <iostream>

#include "core/editor.h"
#include "core/canvas_view.h"

static void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

int main() {
  bitmap16::Sketch blank;
  blank.gridSize = 32;
  bitmap16::Editor editor;
  editor.reset(blank);
  // Overfill the history and verify that exactly the newest 16 changes remain.
  for (int x = 0; x < 20; ++x) {
    editor.setCursor(x, 0);
    require(editor.draw(), "drawing failed");
  }
  for (int x = 19; x >= 4; --x) {
    require(editor.undo(), "missing undo step");
    require(editor.sketch().pixels[0][x] == 0, "undo restored wrong state");
  }
  require(!editor.undo(), "history exceeded 16 steps");
  require(editor.sketch().pixels[0][3] == 1, "oldest retained state incorrect");
  for (int x = 4; x < 20; ++x) {
    require(editor.redo(), "missing redo step");
    require(editor.sketch().pixels[0][x] == 1, "redo restored wrong state");
  }
  require(!editor.redo(), "unexpected redo step");

  require(editor.undo() && editor.undo(), "branch setup failed");
  editor.setCursor(25, 0);
  require(editor.draw(), "branch edit failed");
  require(!editor.canRedo(), "new edit preserved abandoned redo branch");
  require(!editor.draw(), "unchanged pixel created an edit");
  require(editor.undo(), "branch undo failed");
  require(editor.sketch().pixels[0][25] == 0, "branch undo incorrect");
  require(editor.redo(), "no-op draw discarded redo");

  editor.reset(blank);
  require(!editor.canUndo() && !editor.canRedo(), "opening sketch retained history");
  require(editor.draw(), "metadata setup failed");
  uint16_t colors[16] = {};
  colors[0] = 0x1234;
  require(editor.applyPalette(colors, 4), "palette edit failed");
  require(editor.toggleGridSize(), "grid edit failed");
  require(editor.undo(), "grid undo failed");
  require(editor.sketch().gridSize == 32, "grid not restored");
  require(editor.undo(), "palette undo failed");
  require(editor.sketch().paletteSize == 16 && editor.sketch().paletteColors[0] == 0,
          "palette not restored");
  require(editor.undo() && editor.sketch().isEmpty, "empty state not restored");
  require(editor.redo() && editor.redo() && editor.redo(), "metadata redo failed");
  require(editor.sketch().gridSize == 8 && editor.sketch().paletteSize == 4 &&
              editor.sketch().paletteColors[0] == 0x1234,
          "metadata redo incorrect");
  editor.reset(blank);
  require(!editor.canUndo() && !editor.canRedo(), "new sketch retained history");

  editor.setHeldActions(true, false, false);
  for (int x = 0; x < 20; ++x) {
    editor.setCursor(x, 0);
    require(editor.draw(), "held draw failed");
  }
  editor.setHeldActions(false, false, false);
  require(editor.undo() && editor.sketch().isEmpty, "stroke undo did not restore whole stroke");
  require(!editor.canUndo(), "stroke used multiple undo steps");
  require(editor.redo(), "stroke redo failed");
  for (int x = 0; x < 20; ++x) {
    require(editor.sketch().pixels[0][x] == 1, "stroke redo lost pixels");
  }

  editor.setHeldActions(false, true, false);
  for (int x = 0; x < 20; ++x) {
    editor.setCursor(x, 0);
    require(editor.erase(), "held erase failed");
  }
  editor.setHeldActions(false, false, false);
  require(editor.undo(), "erase stroke undo failed");
  require(editor.sketch().pixels[0][0] == 1 && editor.sketch().pixels[0][19] == 1,
          "erase stroke undo restored only part of stroke");
  require(editor.undo() && editor.sketch().isEmpty, "erase stroke used multiple steps");

  // A no-op hold must not discard redo or allocate a history entry.
  editor.setHeldActions(false, true, true);
  require(!editor.erase() && !editor.shift(1, 0), "empty canvas changed");
  editor.setHeldActions(false, false, false);
  require(!editor.canUndo() && editor.canRedo(), "no-op hold changed history");

  editor.reset(blank);
  editor.setHeldActions(true, false, false);
  require(editor.draw(), "first hold failed");
  editor.setHeldActions(false, false, false);
  editor.setHeldActions(true, false, false);
  editor.setCursor(1, 0);
  require(editor.draw(), "second hold failed");
  editor.setHeldActions(false, false, false);
  require(editor.undo() && editor.sketch().pixels[0][0] == 1 &&
              editor.sketch().pixels[0][1] == 0,
          "separate holds were merged");
  require(editor.undo() && editor.sketch().isEmpty, "first hold missing");

  editor.reset(blank);
  require(editor.draw(), "move setup failed");
  editor.setHeldActions(false, false, true);
  for (int i = 0; i < 5; ++i) require(editor.shift(1, 0), "held move failed");
  editor.setHeldActions(false, false, false);
  require(editor.undo() && editor.sketch().pixels[0][0] == 1,
          "move undo did not restore whole gesture");
  require(editor.redo() && editor.sketch().pixels[0][5] == 1,
          "move redo did not restore whole gesture");
  require(editor.undo() && editor.undo() && editor.sketch().isEmpty,
          "move used multiple steps");

  editor.reset(blank);
  editor.setHeldActions(true, false, false);
  require(editor.draw(), "interruption setup failed");
  require(editor.toggleGridSize(), "interruption grid edit failed");
  editor.setCursor(1, 0);
  require(editor.draw(), "drawing after interruption failed");
  require(editor.undo() && editor.sketch().pixels[0][0] == 1 &&
              editor.sketch().pixels[0][1] == 0 && editor.sketch().gridSize == 8,
          "drawing merged across intervening edit");
  require(editor.undo() && editor.sketch().gridSize == 32,
          "intervening edit lost");
  editor.setCursor(2, 0);
  require(editor.draw() && !editor.canRedo(), "drawing after undo kept redo branch");
  require(editor.undo() && editor.sketch().pixels[0][2] == 0,
          "drawing after undo merged with old hold");

  // Mouse/trackpad coordinates share the logical framebuffer and zoom viewport.
  for (const auto dimensions : {std::pair<int, int>{240, 135}, {320, 170}, {320, 200}}) {
    for (const uint8_t grid : {8, 16, 32}) {
      const auto layout = bitmap16::CanvasView::layoutFor(
          dimensions.first, dimensions.second, grid);
      bitmap16::CanvasView::Viewport viewport;
      uint8_t x = 99, y = 99;
      require(bitmap16::CanvasView::cellAtPointer(
          dimensions.first, dimensions.second, grid, viewport,
          layout.gridX, layout.gridY, x, y) && x == 0 && y == 0,
          "pointer missed canvas top-left");
      require(bitmap16::CanvasView::cellAtPointer(
          dimensions.first, dimensions.second, grid, viewport,
          layout.gridX + layout.gridPixels - 1, layout.gridY + layout.gridPixels - 1,
          x, y) && x == grid - 1 && y == grid - 1, "pointer missed canvas bottom-right");
      require(!bitmap16::CanvasView::cellAtPointer(
          dimensions.first, dimensions.second, grid, viewport,
          layout.gridX - 1, layout.gridY, x, y), "pointer accepted left margin");
      require(!bitmap16::CanvasView::cellAtPointer(
          dimensions.first, dimensions.second, grid, viewport,
          layout.gridX + layout.gridPixels, layout.gridY, x, y),
          "pointer accepted right margin");
      if (grid >= 16) {
        viewport.cellSize = layout.cellSize * 2;
        viewport.x = 3;
        viewport.y = 4;
        require(bitmap16::CanvasView::cellAtPointer(
            dimensions.first, dimensions.second, grid, viewport,
            layout.gridX + viewport.cellSize, layout.gridY + viewport.cellSize,
            x, y) && x == 4 && y == 5, "pointer ignored zoom viewport");
      }
    }
  }

  editor.reset(blank);
  editor.setHeldActions(true, false, false);
  require(editor.paintTo(2, 2, false, false), "pointer click failed");
  require(editor.paintTo(12, 12, false, true), "pointer drag failed");
  for (int i = 2; i <= 12; ++i) {
    require(editor.sketch().pixels[i][i] == 1, "fast pointer drag left gaps");
  }
  require(editor.sketch().pixels[0][0] == 0, "pointer click connected to old cursor");
  editor.setHeldActions(false, false, false);
  require(editor.undo() && editor.sketch().isEmpty && !editor.canUndo(),
          "pointer drag used multiple undo steps");
  require(editor.redo(), "pointer drag redo failed");
  editor.setHeldActions(false, true, false);
  editor.setCursor(2, 2);
  require(editor.paintTo(12, 12, true, true) && editor.sketch().isEmpty,
          "pointer erase left gaps");
  editor.setHeldActions(false, false, false);
  require(editor.undo() && editor.sketch().pixels[2][2] == 1 &&
              editor.sketch().pixels[12][12] == 1, "pointer erase undo incomplete");
}
