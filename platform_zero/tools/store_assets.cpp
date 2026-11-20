// Render store assets with the same core views and embedded art as the app.
#include <filesystem>
#include <iostream>
#include <vector>
#include "core/canvas_view.h"
#include "core/palette_view.h"
#include "core/sketch.h"
#include "icons.h"
#include "cartridge_graphic.h"
#include "palettes.h"
#include "png_export.h"

namespace {
bool save(const bitmap16::Canvas& canvas, const std::filesystem::path& path) {
  std::vector<uint8_t> rgb;
  for (int i = 0; i < canvas.width() * canvas.height(); ++i) {
    const auto color = canvas.pixels()[i];
    rgb.push_back(((color >> 11) & 31) * 255 / 31);
    rgb.push_back(((color >> 5) & 63) * 255 / 63);
    rgb.push_back((color & 31) * 255 / 31);
  }
  return bitmap16::Desktop::PngExport::writeRgb(path, rgb, canvas.width(), canvas.height());
}
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Usage: bitmap16dx_store_assets OUTPUT_DIRECTORY LAUNCHER_ICON\n";
    return 1;
  }
  const std::filesystem::path output = argv[1];
  std::filesystem::create_directories(output);
  bitmap16::Canvas canvas;
  if (!canvas.create(256, 256)) return 1;
  canvas.fillScreen(0x0000);
  // Rasterize the existing code-native pencil at an integer scale.
  for (int y = 0; y < 24; ++y) {
    for (int x = 0; x < 24; ++x) {
      const int pixel = y * 24 + x;
      const auto index = (ICON_DRAW[pixel / 4] >> (6 - 2 * (pixel % 4))) & 3;
      if (index) canvas.fillRect(32 + x * 8, 32 + y * 8, 8, 8,
                                index == 1 ? 0x0000 : 0xffff);
    }
  }
  if (!save(canvas, output / "icon.png")) return 1;
  // Render at an integer scale with transparent padding to keep pixel edges crisp.
  constexpr int launcherSize = 128;
  constexpr int launcherScale = 5;
  constexpr int launcherPadding = (launcherSize - 24 * launcherScale) / 2;
  std::vector<uint8_t> launcher(launcherSize * launcherSize * 4, 0);
  for (int y = 0; y < launcherSize; ++y) {
    for (int x = 0; x < launcherSize; ++x) {
      const int sourceX = x - launcherPadding;
      const int sourceY = y - launcherPadding;
      if (sourceX < 0 || sourceY < 0 || sourceX >= 24 * launcherScale ||
          sourceY >= 24 * launcherScale) continue;
      const int pixel = (sourceY / launcherScale) * 24 + sourceX / launcherScale;
      const auto index = (ICON_DRAW[pixel / 4] >> (6 - 2 * (pixel % 4))) & 3;
      const int offset = (y * launcherSize + x) * 4;
      if (index) {
        const uint8_t color = index == 1 ? 0 : 255;
        launcher[offset] = launcher[offset + 1] = launcher[offset + 2] = color;
        launcher[offset + 3] = 255;
      }
    }
  }
  if (!bitmap16::Desktop::PngExport::writeRgba(argv[2], launcher, launcherSize, launcherSize)) return 1;
  if (!canvas.create(320, 170)) return 1;
  bitmap16::Sketch sketch;
  bitmap16::initializeSketch(sketch, 16, PALETTE_SWEETIE16, 16);
  // A small original mug drawing, using the normal indexed sketch format.
  constexpr const char* rows[] = {
    "................", ".....e..e.......", "....e..e........", ".....e..e.......",
    "................", "...dddddddd.....", "...dccccccd.dd..", "...dccccccddd.d.",
    "...dccccccddd.d.", "...dbbbcccdddd..", "...dbbbcccd.....", "...dbbbbbbd.....",
    "....dddddd......", "................", "..fffffffffff...", "................"};
  for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
    const char color = rows[y][x];
    if (color != '.') sketch.pixels[y][x] = color <= '9' ? color - '0' : color - 'a' + 10;
  }
  sketch.isEmpty = false;
  bitmap16::CanvasView::Assets assets{};
  assets.draw = {ICON_DRAW, 24, 24};
  assets.erase = {ICON_ERASE, 24, 24};
  assets.fill = {ICON_FILL, 24, 24};
  assets.cursor = {ICON_CANVAS_CURSOR, ICON_CANVAS_CURSOR_WIDTH, ICON_CANVAS_CURSOR_HEIGHT};
  assets.cursorOffsetX = CURSOR_OFFSET_X;
  assets.cursorOffsetY = CURSOR_OFFSET_Y;
  bitmap16::CanvasView::State state{};
  state.pixels = sketch.pixels;
  state.gridSize = 16;
  state.paletteColors = sketch.paletteColors;
  state.paletteSize = 16;
  state.cursorX = 9;
  state.cursorY = 11;
  state.selectedColor = 12;
  bitmap16::CanvasView::render(canvas, state,
      {0xd69b, 0xef7e, 0xffff, 0xc63a, 0, 0x94b3, 0xd69b, 0, 0xffff, false}, &assets);
  if (!save(canvas, output / "drawing.png")) return 1;
  bitmap16::CanvasView::render(canvas, state,
      {0x0861, 0x10a2, 0x2965, 0x0020, 0xffff, 0x94b3, 0x0861, 0, 0xd69b, true}, &assets);
  if (!save(canvas, output / "drawing-dark.png")) return 1;
  std::vector<bitmap16::PaletteView::Entry> entries;
  for (int i = 0; i < NUM_PALETTES; ++i) {
    entries.push_back({PALETTE_CATALOG[i], PALETTE_NAMES[i], PALETTE_SIZES[i]});
  }
  const bitmap16::PaletteView::Catalog catalog{entries.data(), NUM_PALETTES};
  bitmap16::PaletteView::State palettes;
  bitmap16::PaletteView::reset(palettes, catalog);
  bitmap16::PaletteView::selectCatalogIndex(palettes, 1);
  bitmap16::PaletteView::render(canvas, palettes, catalog, 0,
      {0xd69b, 0, 0x94b3, false}, CARTRIDGE_GRAPHIC, "[I]mport|Browse [L]ospec");
  if (!save(canvas, output / "palettes.png")) return 1;
  std::cout << "Store icon and three 320x170 screenshots written to " << output << '\n';
}
