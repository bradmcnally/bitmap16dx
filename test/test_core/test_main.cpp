#include <unity.h>

#include <algorithm>
#include <cstring>
#include <initializer_list>

#include "core/app.h"
#include "core/charging_view.h"
#include "core/clock.h"
#include "core/canvas.h"
#include "core/canvas_view.h"
#include "core/editor.h"
#include "core/help_view.h"
#include "core/input.h"
#include "core/led_mapping.h"
#include "core/memory_view.h"
#include "core/palette.h"
#include "core/palette_view.h"
#include "core/preview_view.h"
#include "core/shake_detector.h"
#include "core/sketch.h"
#include "core/sketch_codec.h"
#include "core/settings.h"
#include "core/settings_view.h"

using bitmap16::Editor;
using bitmap16::Sketch;

namespace {

int appFrameCalls = 0;

void countAppFrame() {
  ++appFrameCalls;
}

Sketch makeSketch(uint8_t gridSize = 8, uint8_t paletteSize = 16) {
  Sketch sketch;
  sketch.gridSize = gridSize;
  sketch.paletteSize = paletteSize;
  for (uint8_t i = 0; i < 16; ++i) {
    sketch.paletteColors[i] = static_cast<uint16_t>(0x1000 + i);
  }
  return sketch;
}

bitmap16::RawInputState rawKeys(
    const char* keys,
    bool changed = true,
    bool pressed = true) {
  bitmap16::RawInputState raw;
  raw.keyChanged = changed;
  raw.keyPressed = pressed;
  for (std::size_t i = 0;
       keys[i] != '\0' && i < bitmap16::RawInputState::kMaxKeys;
       ++i) {
    raw.keys[raw.keyCount++] = keys[i];
  }
  return raw;
}

void test_editor_draw_erase_and_undo() {
  Editor editor;
  editor.reset(makeSketch());
  editor.setCursor(2, 3);
  editor.setSelectedColor(4);

  TEST_ASSERT_TRUE(editor.draw());
  TEST_ASSERT_EQUAL_UINT8(4, editor.sketch().pixels[3][2]);
  TEST_ASSERT_TRUE(editor.canUndo());
  TEST_ASSERT_TRUE(editor.undo());
  TEST_ASSERT_EQUAL_UINT8(0, editor.sketch().pixels[3][2]);
  TEST_ASSERT_FALSE(editor.canUndo());
  TEST_ASSERT_TRUE(editor.canRedo());
  TEST_ASSERT_TRUE(editor.redo());
  TEST_ASSERT_EQUAL_UINT8(4, editor.sketch().pixels[3][2]);
  TEST_ASSERT_TRUE(editor.canUndo());
  TEST_ASSERT_FALSE(editor.canRedo());
  TEST_ASSERT_TRUE(editor.undo());
  TEST_ASSERT_EQUAL_UINT8(0, editor.sketch().pixels[3][2]);

  editor.draw();
  TEST_ASSERT_FALSE(editor.canRedo());
  TEST_ASSERT_TRUE(editor.erase());
  TEST_ASSERT_EQUAL_UINT8(0, editor.sketch().pixels[3][2]);
  TEST_ASSERT_TRUE(editor.undo());
  TEST_ASSERT_EQUAL_UINT8(4, editor.sketch().pixels[3][2]);
}

void test_canvas_allocation_fill_and_release() {
  bitmap16::Canvas canvas;
  TEST_ASSERT_FALSE(canvas.create(0, 10));
  TEST_ASSERT_TRUE(canvas.create(10, 8));
  TEST_ASSERT_TRUE(canvas.isValid());
  TEST_ASSERT_EQUAL_INT(10, canvas.width());
  TEST_ASSERT_EQUAL_INT(8, canvas.height());

  canvas.fillScreen(0x1234);
  TEST_ASSERT_EQUAL_HEX16(0x1234, canvas.readPixel(0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x1234, canvas.readPixel(9, 7));
  TEST_ASSERT_EQUAL_HEX16(0xabcd, canvas.readPixel(-1, 0, 0xabcd));

  canvas.release();
  TEST_ASSERT_FALSE(canvas.isValid());
  TEST_ASSERT_EQUAL_INT(0, canvas.width());
}

void test_canvas_clips_rectangles_and_pixels() {
  bitmap16::Canvas canvas;
  TEST_ASSERT_TRUE(canvas.create(6, 5));
  canvas.fillScreen(0);
  canvas.fillRect(-2, -1, 4, 3, 0x1111);
  canvas.fillRect(20, 20, 2, 2, 0xffff);
  canvas.drawPixel(5, 4, 0x2222);
  canvas.drawPixel(6, 4, 0xffff);

  TEST_ASSERT_EQUAL_HEX16(0x1111, canvas.readPixel(0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x1111, canvas.readPixel(1, 1));
  TEST_ASSERT_EQUAL_HEX16(0, canvas.readPixel(2, 1));
  TEST_ASSERT_EQUAL_HEX16(0x2222, canvas.readPixel(5, 4));
}

void test_canvas_view_layout_is_resolution_and_grid_aware() {
  const bitmap16::CanvasView::Layout small =
      bitmap16::CanvasView::layoutFor(240, 135, 8);
  TEST_ASSERT_EQUAL_INT(128, small.gridPixels);
  TEST_ASSERT_EQUAL_INT(16, small.cellSize);
  TEST_ASSERT_EQUAL_INT(56, small.gridX);
  TEST_ASSERT_EQUAL_INT(15, small.toolsX);
  TEST_ASSERT_EQUAL_INT(203, small.paletteX);
  TEST_ASSERT_EQUAL_INT(15, small.statusX);

  const bitmap16::CanvasView::Layout large =
      bitmap16::CanvasView::layoutFor(320, 170, 16);
  TEST_ASSERT_EQUAL_INT(128, large.gridPixels);
  TEST_ASSERT_EQUAL_INT(8, large.cellSize);
  TEST_ASSERT_EQUAL_INT(96, large.gridX);
  TEST_ASSERT_EQUAL_INT(55, large.toolsX);
  TEST_ASSERT_EQUAL_INT(243, large.paletteX);
  TEST_ASSERT_EQUAL_INT(55, large.statusX);
}

void test_canvas_view_renders_editor_at_both_target_sizes() {
  uint8_t pixels[16][16] = {};
  uint16_t colors[16] = {};
  colors[0] = 0xf800;
  colors[1] = 0x07e0;
  pixels[2][3] = 2;
  const bitmap16::CanvasView::State state = {
      pixels, 8, colors, 2, 3, 2, 2, true, false, "COLOR 2", 75};
  const bitmap16::CanvasView::Theme theme = {
      0x1111, 0x2222, 0xeeee, 0x0808, 0xffff, 0x8888, 0x4444,
      0x0000, 0xffff, false};

  for (const int width : {240, 320}) {
    const int height = width == 240 ? 135 : 170;
    bitmap16::Canvas canvas;
    TEST_ASSERT_TRUE(canvas.create(width, height));
    bitmap16::CanvasView::render(canvas, state, theme);
    const bitmap16::CanvasView::Layout layout =
        bitmap16::CanvasView::layoutFor(width, height, 8);
    TEST_ASSERT_NOT_EQUAL(
        theme.background,
        canvas.readPixel(layout.gridX + 3, layout.gridY + 3));
    TEST_ASSERT_EQUAL_HEX16(
        theme.text,
        canvas.readPixel(
            layout.gridX + 3 * layout.cellSize,
            layout.gridY + 2 * layout.cellSize));
    TEST_ASSERT_EQUAL_HEX16(
        colors[1],
        canvas.readPixel(
            layout.paletteX + 4,
            layout.gridY + layout.paletteSwatchSize + 4));
  }
}

void test_canvas_draws_lines_and_rectangles() {
  bitmap16::Canvas canvas;
  TEST_ASSERT_TRUE(canvas.create(8, 8));
  canvas.fillScreen(0);
  canvas.drawLine(0, 0, 3, 3, 0xaaaa);
  canvas.drawRect(4, 1, 3, 4, 0xbbbb);

  for (int i = 0; i < 4; ++i) {
    TEST_ASSERT_EQUAL_HEX16(0xaaaa, canvas.readPixel(i, i));
  }
  TEST_ASSERT_EQUAL_HEX16(0xbbbb, canvas.readPixel(4, 1));
  TEST_ASSERT_EQUAL_HEX16(0xbbbb, canvas.readPixel(6, 4));
  TEST_ASSERT_EQUAL_HEX16(0, canvas.readPixel(5, 2));
}

void test_canvas_pushes_clipped_and_byte_swapped_images() {
  const uint16_t image[] = {0x1122, 0x3344, 0x5566, 0x7788};
  bitmap16::Canvas canvas;
  TEST_ASSERT_TRUE(canvas.create(3, 3));
  canvas.fillScreen(0);

  canvas.pushImage(2, 1, 2, 2, image);
  TEST_ASSERT_EQUAL_HEX16(0x1122, canvas.readPixel(2, 1));
  TEST_ASSERT_EQUAL_HEX16(0x5566, canvas.readPixel(2, 2));

  canvas.pushImage(0, 0, 2, 2, image, true);
  TEST_ASSERT_EQUAL_HEX16(0x2211, canvas.readPixel(0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x8877, canvas.readPixel(1, 1));
}

void test_canvas_draws_scaled_and_aligned_text() {
  bitmap16::Canvas canvas;
  TEST_ASSERT_TRUE(canvas.create(20, 20));
  canvas.fillScreen(0);
  canvas.setTextColor(0xffff);
  canvas.setTextAlign(bitmap16::TextAlign::Center);
  canvas.setTextSize(2);

  TEST_ASSERT_EQUAL_INT(12, canvas.textWidth("A"));
  canvas.drawString("A", 10, 0);
  TEST_ASSERT_EQUAL_HEX16(0xffff, canvas.readPixel(4, 2));
  TEST_ASSERT_EQUAL_HEX16(0xffff, canvas.readPixel(5, 3));
}

void test_help_view_navigation_clamps_to_available_items() {
  bitmap16::HelpView::State state;
  TEST_ASSERT_EQUAL_INT(21, bitmap16::HelpView::itemCount(false, true));
  TEST_ASSERT_EQUAL_INT(23, bitmap16::HelpView::itemCount(true, true));
  TEST_ASSERT_EQUAL_INT(20, bitmap16::HelpView::itemCount(false, false));
  TEST_ASSERT_FALSE(
      bitmap16::HelpView::moveCursor(state, -1, false, true));
  TEST_ASSERT_TRUE(
      bitmap16::HelpView::moveCursor(state, 30, false, true));
  TEST_ASSERT_EQUAL_INT(20, state.cursor);
  TEST_ASSERT_FALSE(
      bitmap16::HelpView::moveCursor(state, 1, false, true));
  TEST_ASSERT_TRUE(
      bitmap16::HelpView::moveCursor(state, -1, false, true));
  TEST_ASSERT_EQUAL_INT(19, state.cursor);
}

void test_help_view_renders_and_scrolls_at_both_target_sizes() {
  bitmap16::HelpView::Theme theme = {0x1111, 0xffff, 0x7777};
  for (const int width : {240, 320}) {
    const int height = width == 240 ? 135 : 170;
    bitmap16::Canvas canvas;
    TEST_ASSERT_TRUE(canvas.create(width, height));
    bitmap16::HelpView::State state;
    state.cursor = bitmap16::HelpView::itemCount(true, true) - 1;

    bitmap16::HelpView::render(canvas, state, theme, true, true);

    TEST_ASSERT_GREATER_THAN_INT(0, state.scrollOffset);
    TEST_ASSERT_EQUAL_HEX16(theme.text, canvas.readPixel(4, 5));
    TEST_ASSERT_EQUAL_HEX16(
        theme.background,
        canvas.readPixel(width - 1, height - 1));
  }
}

void test_settings_view_navigation_and_actions_are_portable() {
  bitmap16::SettingsView::State state;
  bitmap16::Settings settings;
  TEST_ASSERT_EQUAL_INT(
      6, bitmap16::SettingsView::itemCount(false, true, true));
  TEST_ASSERT_EQUAL_INT(
      7, bitmap16::SettingsView::itemCount(true, true, true));
  TEST_ASSERT_EQUAL_INT(
      3, bitmap16::SettingsView::itemCount(false, false, false));

  TEST_ASSERT_TRUE(
      bitmap16::SettingsView::activate(
          state, settings, false, true, true) ==
      bitmap16::SettingsView::Action::ThemeChanged);
  TEST_ASSERT_TRUE(settings.theme == bitmap16::ThemeId::Dark);

  state.cursor = 5;
  TEST_ASSERT_TRUE(
      bitmap16::SettingsView::activate(
          state, settings, false, true, true) ==
      bitmap16::SettingsView::Action::ShakeUndoChanged);
  TEST_ASSERT_TRUE(settings.shakeUndoEnabled);
  TEST_ASSERT_FALSE(
      bitmap16::SettingsView::moveCursor(
          state, 1, false, true, true));

  state.cursor = 6;
  TEST_ASSERT_TRUE(
      bitmap16::SettingsView::activate(
          state, settings, true, true, true) ==
      bitmap16::SettingsView::Action::BluetoothRequested);
}

void test_settings_view_renders_at_both_target_sizes() {
  const bitmap16::SettingsView::Theme theme = {
      0x1111,
      0xffff,
      0x7777,
  };
  for (const int width : {240, 320}) {
    const int height = width == 240 ? 135 : 170;
    bitmap16::Canvas canvas;
    TEST_ASSERT_TRUE(canvas.create(width, height));
    bitmap16::Settings settings;
    bitmap16::SettingsView::State state;
    state.cursor =
        bitmap16::SettingsView::itemCount(false, true, true) - 1;

    bitmap16::SettingsView::render(
        canvas,
        state,
        settings,
        theme,
        false,
        true,
        true,
        nullptr,
        "Saved");

    TEST_ASSERT_EQUAL_HEX16(theme.text, canvas.readPixel(4, 5));
    TEST_ASSERT_EQUAL_HEX16(
        theme.background,
        canvas.readPixel(width - 1, height - 1));
    if (width == 240) {
      TEST_ASSERT_GREATER_THAN_INT(0, state.scrollOffset);
    }
  }
}

void test_preview_view_background_selection_is_bounded() {
  bitmap16::PreviewView::State state;
  TEST_ASSERT_TRUE(bitmap16::PreviewView::selectBackground(state, 2));
  TEST_ASSERT_TRUE(
      state.background == bitmap16::PreviewView::Background::Gray);
  TEST_ASSERT_FALSE(bitmap16::PreviewView::selectBackground(state, 2));
  TEST_ASSERT_FALSE(bitmap16::PreviewView::selectBackground(state, -1));
  TEST_ASSERT_FALSE(bitmap16::PreviewView::selectBackground(state, 4));
  TEST_ASSERT_TRUE(
      state.background == bitmap16::PreviewView::Background::Gray);
}

void test_preview_view_renders_indexed_pixels_at_both_target_sizes() {
  uint8_t pixels[16][16] = {};
  uint16_t palette[16] = {};
  pixels[0][0] = 1;
  pixels[0][1] = 2;
  pixels[0][2] = 3;
  palette[0] = 0xf800;
  palette[1] = 0x07e0;
  palette[2] = 0x001f;
  const bitmap16::PreviewView::Image image = {
      pixels,
      16,
      palette,
      2,
  };
  const bitmap16::PreviewView::Theme theme = {
      0x0000,
      0xffff,
      0x7777,
      0x1111,
  };

  for (const int width : {240, 320}) {
    const int height = width == 240 ? 135 : 170;
    bitmap16::Canvas canvas;
    TEST_ASSERT_TRUE(canvas.create(width, height));
    bitmap16::PreviewView::State state;
    bitmap16::PreviewView::selectBackground(state, 2);

    bitmap16::PreviewView::render(canvas, state, image, theme);

    const int viewX = (width - 128) / 2;
    const int viewY = (height - 128 + 1) / 2;
    TEST_ASSERT_EQUAL_HEX16(palette[0], canvas.readPixel(viewX, viewY));
    TEST_ASSERT_EQUAL_HEX16(
        palette[1], canvas.readPixel(viewX + 8, viewY));
    TEST_ASSERT_EQUAL_HEX16(
        theme.gray, canvas.readPixel(viewX + 16, viewY));
    TEST_ASSERT_EQUAL_HEX16(theme.gray, canvas.readPixel(0, 0));
  }
}

void test_charging_view_initializes_and_bounces_within_bounds() {
  uint8_t iconData[4][144] = {};
  const uint8_t* const icons[4] = {
      iconData[0], iconData[1], iconData[2], iconData[3]};
  bitmap16::ChargingView::State state;
  bitmap16::ChargingView::initialize(
      state, 240, 135, 1234, icons, 140, false);
  TEST_ASSERT_EQUAL_INT(100, state.batteryPercent);
  TEST_ASSERT_FALSE(state.sketchAvailable);

  state.items[0] = {0.0f, 0.0f, -1.0f, -1.0f, iconData[0]};
  state.items[1] = {80.0f, 0.0f, 1.0f, 1.0f, iconData[1]};
  state.items[2] = {120.0f, 60.0f, 1.0f, 1.0f, iconData[2]};
  state.items[3] = {180.0f, 100.0f, 1.0f, 1.0f, iconData[3]};

  bitmap16::ChargingView::update(state, 240, 135);

  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.items[0].x);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, state.items[0].y);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, state.items[0].dx);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, state.items[0].dy);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(186.0f, state.items[3].x);
  TEST_ASSERT_LESS_OR_EQUAL_FLOAT(111.0f, state.items[3].y);
}

void test_charging_view_renders_icons_sketch_and_battery_at_target_sizes() {
  uint8_t icons[4][144];
  std::memset(icons, 0xaa, sizeof(icons));
  uint8_t sketchPixels[16][16] = {};
  uint16_t sketchPalette[16] = {};
  sketchPixels[0][0] = 1;
  sketchPalette[0] = 0xf800;
  const bitmap16::ChargingView::SketchImage sketch = {
      sketchPixels, 16, sketchPalette, 1};
  const bitmap16::ChargingView::Theme theme = {
      0x0000, 0x1111, 0xffff, 0xffff};

  for (const int width : {240, 320}) {
    const int height = width == 240 ? 135 : 170;
    bitmap16::Canvas canvas;
    TEST_ASSERT_TRUE(canvas.create(width, height));
    bitmap16::ChargingView::State state;
    state.batteryPercent = 78;
    state.sketchAvailable = true;
    for (int item = 0; item < 4; ++item) {
      state.items[item].x = static_cast<float>(item * 30);
      state.items[item].y = 4.0f;
      state.items[item].icon = icons[item];
    }
    state.items[bitmap16::ChargingView::kSketchItem].x = 10.0f;
    state.items[bitmap16::ChargingView::kSketchItem].y = 60.0f;

    bitmap16::ChargingView::render(canvas, state, theme, &sketch);

    TEST_ASSERT_EQUAL_HEX16(theme.iconLight, canvas.readPixel(0, 4));
    TEST_ASSERT_EQUAL_HEX16(sketchPalette[0], canvas.readPixel(10, 60));
    TEST_ASSERT_EQUAL_HEX16(
        theme.background, canvas.readPixel(width - 1, height - 1));
  }
}

void test_palette_view_filters_navigates_and_animates_selection() {
  uint16_t colors[4][16] = {};
  bitmap16::PaletteView::Entry entries[4] = {
      {colors[0], "A", 4, false},
      {colors[1], "B", 8, false},
      {colors[2], "C", 16, false},
      {colors[3], "D", 8, true},
  };
  const bitmap16::PaletteView::Catalog catalog = {entries, 4};
  bitmap16::PaletteView::State state;
  bitmap16::PaletteView::reset(state, catalog);
  TEST_ASSERT_EQUAL_INT(4, state.filteredCount);
  TEST_ASSERT_TRUE(bitmap16::PaletteView::moveCursor(state, 2));
  TEST_ASSERT_EQUAL_INT(2, state.cursor);

  TEST_ASSERT_TRUE(
      bitmap16::PaletteView::toggleSizeFilter(state, catalog, 8));
  TEST_ASSERT_EQUAL_INT(2, state.filteredCount);
  TEST_ASSERT_EQUAL_INT(0, state.cursor);
  TEST_ASSERT_TRUE(
      bitmap16::PaletteView::toggleUserFilter(state, catalog));
  TEST_ASSERT_EQUAL_INT(1, state.filteredCount);
  TEST_ASSERT_EQUAL_INT(3, bitmap16::PaletteView::selectedCatalogIndex(state));

  TEST_ASSERT_TRUE(bitmap16::PaletteView::beginSelection(state));
  TEST_ASSERT_TRUE(
      bitmap16::PaletteView::advance(state, 1.0f, 0.5f) ==
      bitmap16::PaletteView::AnimationResult::Animating);
  TEST_ASSERT_TRUE(
      bitmap16::PaletteView::advance(state, 1.0f, 0.5f) ==
      bitmap16::PaletteView::AnimationResult::SelectionComplete);
}

void test_palette_view_renders_carousel_at_both_target_sizes() {
  uint16_t colors[2][16] = {};
  uint16_t cartridge[80 * 92];
  std::fill(
      std::begin(cartridge), std::end(cartridge), 0x3456);
  for (int color = 0; color < 16; ++color) {
    colors[0][color] = static_cast<uint16_t>(0x1000 + color);
    colors[1][color] = static_cast<uint16_t>(0x2000 + color);
  }
  bitmap16::PaletteView::Entry entries[2] = {
      {colors[0], "FIRST", 4, false},
      {colors[1], "SECOND", 16, true},
  };
  const bitmap16::PaletteView::Catalog catalog = {entries, 2};
  const bitmap16::PaletteView::Theme theme = {
      0x7777, 0xffff, 0x1111, false};

  for (const int width : {240, 320}) {
    const int height = width == 240 ? 135 : 170;
    bitmap16::Canvas canvas;
    TEST_ASSERT_TRUE(canvas.create(width, height));
    bitmap16::PaletteView::State state;
    bitmap16::PaletteView::reset(state, catalog);

    bitmap16::PaletteView::render(
        canvas, state, catalog, 0, theme, cartridge, "Ready");

    const int swatchX = width / 2 - 32;
    const int swatchY = std::max(66, height / 2 - 1) - 40;
    const int cartridgeTop = std::max(66, height / 2 - 1) - 46;
    TEST_ASSERT_EQUAL_HEX16(
        0x3456, canvas.readPixel(width / 2, cartridgeTop));
    TEST_ASSERT_EQUAL_HEX16(colors[0][0], canvas.readPixel(
        swatchX + 2, swatchY));
    TEST_ASSERT_EQUAL_HEX16(theme.text, canvas.readPixel(4, 5));
    TEST_ASSERT_EQUAL_HEX16(
        theme.background, canvas.readPixel(width - 1, height - 1));
  }
}

void test_memory_view_navigation_and_scroll_are_resolution_aware() {
  bitmap16::MemoryView::State state;
  TEST_ASSERT_EQUAL_INT(4, bitmap16::MemoryView::columnCount(240));
  TEST_ASSERT_EQUAL_INT(5, bitmap16::MemoryView::columnCount(320));
  TEST_ASSERT_TRUE(
      bitmap16::MemoryView::moveCursor(state, 0, 1, 8, 240));
  TEST_ASSERT_EQUAL_INT(4, state.cursor);
  TEST_ASSERT_TRUE(
      bitmap16::MemoryView::moveCursor(state, 1, 0, 8, 240));
  TEST_ASSERT_EQUAL_INT(5, state.cursor);
  state.cursor = 8;
  bitmap16::MemoryView::advance(state, 8, 240, 135, 0.016f, 1.0f);
  TEST_ASSERT_GREATER_THAN_INT(0, state.scrollOffset);
  TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, state.scrollPosition);
  bitmap16::MemoryView::clamp(state, 2);
  TEST_ASSERT_EQUAL_INT(2, state.cursor);
}

void test_memory_view_renders_new_and_sketch_tiles_at_target_sizes() {
  uint8_t pixels[16][16] = {};
  uint16_t palette[16] = {};
  pixels[0][0] = 1;
  palette[0] = 0xf800;
  const bitmap16::MemoryView::Entry entries[1] = {
      {pixels, 16, palette, 1, true},
  };
  const bitmap16::MemoryView::Catalog catalog = {entries, 1};
  const bitmap16::MemoryView::Theme theme = {
      0x7777, 0x1111, 0xffff, 0x0000, 0x2222, 0xffe0};
  uint8_t selector[16 * 16 / 4] = {};
  selector[0] = 0x80;
  const bitmap16::MemoryView::Assets assets = {
      selector, 16, 16};

  for (const int width : {240, 320}) {
    const int height = width == 240 ? 135 : 170;
    bitmap16::Canvas canvas;
    TEST_ASSERT_TRUE(canvas.create(width, height));
    bitmap16::MemoryView::State state;

    bitmap16::MemoryView::render(
        canvas, state, catalog, theme, "Ready", &assets);

    const int columns = bitmap16::MemoryView::columnCount(width);
    const int startX =
        (width - (columns * 48 + (columns - 1) * 8)) / 2;
    TEST_ASSERT_EQUAL_HEX16(
        theme.background, canvas.readPixel(startX, 19));
    TEST_ASSERT_EQUAL_HEX16(
        theme.thumbnail, canvas.readPixel(startX + 2, 19));
    TEST_ASSERT_EQUAL_HEX16(
        theme.selectionLight, canvas.readPixel(startX - 4, 15));
    TEST_ASSERT_EQUAL_HEX16(
        palette[0], canvas.readPixel(startX + 58, 19));
    TEST_ASSERT_EQUAL_HEX16(theme.text, canvas.readPixel(4, 5));
    TEST_ASSERT_EQUAL_HEX16(
        theme.background, canvas.readPixel(width - 1, height - 1));
  }
}

void test_editor_flood_fill_is_four_way_and_bounded() {
  Sketch sketch = makeSketch(8);
  sketch.pixels[0][0] = 2;
  sketch.pixels[0][1] = 2;
  sketch.pixels[1][0] = 2;
  sketch.pixels[1][1] = 3;
  sketch.pixels[2][2] = 2;

  Editor editor;
  editor.reset(sketch);
  editor.setSelectedColor(5);

  TEST_ASSERT_TRUE(editor.floodFill());
  TEST_ASSERT_EQUAL_UINT8(5, editor.sketch().pixels[0][0]);
  TEST_ASSERT_EQUAL_UINT8(5, editor.sketch().pixels[0][1]);
  TEST_ASSERT_EQUAL_UINT8(5, editor.sketch().pixels[1][0]);
  TEST_ASSERT_EQUAL_UINT8(3, editor.sketch().pixels[1][1]);
  TEST_ASSERT_EQUAL_UINT8(2, editor.sketch().pixels[2][2]);
}

void test_editor_shift_wraps_active_grid() {
  Sketch sketch = makeSketch(8);
  sketch.pixels[0][7] = 6;
  sketch.pixels[7][0] = 9;
  sketch.pixels[12][12] = 11;

  Editor editor;
  editor.reset(sketch);
  TEST_ASSERT_TRUE(editor.shift(1, 1));

  TEST_ASSERT_EQUAL_UINT8(6, editor.sketch().pixels[1][0]);
  TEST_ASSERT_EQUAL_UINT8(9, editor.sketch().pixels[0][1]);
  TEST_ASSERT_EQUAL_UINT8(11, editor.sketch().pixels[12][12]);
}

void test_editor_repeated_shift_preserves_one_undo_snapshot() {
  Sketch sketch = makeSketch(8);
  sketch.pixels[0][0] = 3;

  Editor editor;
  editor.reset(sketch);
  TEST_ASSERT_TRUE(editor.shift(1, 0));
  TEST_ASSERT_TRUE(editor.shift(1, 0, false));
  TEST_ASSERT_EQUAL_UINT8(3, editor.sketch().pixels[0][2]);

  TEST_ASSERT_TRUE(editor.undo());
  TEST_ASSERT_EQUAL_UINT8(3, editor.sketch().pixels[0][0]);
  TEST_ASSERT_EQUAL_UINT8(0, editor.sketch().pixels[0][2]);
}

void test_editor_toggle_grid_clamps_cursor_and_undo_restores_grid() {
  Editor editor;
  editor.reset(makeSketch(16));
  editor.setCursor(15, 14);

  editor.toggleGridSize();
  TEST_ASSERT_EQUAL_UINT8(8, editor.sketch().gridSize);
  TEST_ASSERT_EQUAL_UINT8(7, editor.cursorX());
  TEST_ASSERT_EQUAL_UINT8(7, editor.cursorY());

  TEST_ASSERT_TRUE(editor.undo());
  TEST_ASSERT_EQUAL_UINT8(16, editor.sketch().gridSize);
}

void test_palette_index_collapse_matches_existing_rules() {
  TEST_ASSERT_EQUAL_UINT8(0, bitmap16::Palette::collapseIndex(0, 4));
  TEST_ASSERT_EQUAL_UINT8(1, bitmap16::Palette::collapseIndex(5, 4));
  TEST_ASSERT_EQUAL_UINT8(4, bitmap16::Palette::collapseIndex(16, 4));
  TEST_ASSERT_EQUAL_UINT8(1, bitmap16::Palette::collapseIndex(9, 8));
  TEST_ASSERT_EQUAL_UINT8(8, bitmap16::Palette::collapseIndex(16, 8));
}

void test_sketch_initialization_repeats_small_palettes() {
  const uint16_t colors[] = {0x1111, 0x2222, 0x3333, 0x4444};
  Sketch sketch;
  bitmap16::initializeSketch(sketch, 8, colors, 4);

  TEST_ASSERT_EQUAL_UINT8(8, sketch.gridSize);
  TEST_ASSERT_EQUAL_UINT8(4, sketch.paletteSize);
  TEST_ASSERT_EQUAL_HEX16(0x1111, sketch.paletteColors[0]);
  TEST_ASSERT_EQUAL_HEX16(0x1111, sketch.paletteColors[4]);
  TEST_ASSERT_EQUAL_HEX16(0x4444, sketch.paletteColors[15]);
  TEST_ASSERT_TRUE(sketch.isEmpty);
}

void test_palette_parser_accepts_lospec_format_and_repeats_colors() {
  const char text[] =
      "// sample palette\n"
      "#112233\n"
      "445566\r\n"
      "  778899  \n"
      "aabbcc\n";
  bitmap16::Palette::Parsed parsed;

  TEST_ASSERT_TRUE(
      bitmap16::Palette::parseLospecHex(text, std::strlen(text), parsed));
  TEST_ASSERT_EQUAL_UINT8(4, parsed.size);
  TEST_ASSERT_EQUAL_HEX16(
      bitmap16::Palette::rgb888ToRgb565(0x11, 0x22, 0x33),
      parsed.colors[0]);
  TEST_ASSERT_EQUAL_HEX16(parsed.colors[0], parsed.colors[4]);
  TEST_ASSERT_EQUAL_HEX16(parsed.colors[3], parsed.colors[15]);
}

void test_palette_parser_rejects_unsupported_count() {
  const char text[] = "000000\nffffff\nff0000\n";
  bitmap16::Palette::Parsed parsed;
  TEST_ASSERT_FALSE(
      bitmap16::Palette::parseLospecHex(text, std::strlen(text), parsed));
}

void test_codec_v2_round_trip_is_exact() {
  Sketch original = makeSketch(16, 8);
  original.pixels[0][0] = 1;
  original.pixels[15][15] = 8;
  original.isEmpty = false;

  uint8_t bytes[bitmap16::SketchCodec::kCurrentFileSize] = {};
  std::size_t written = 0;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::SketchCodec::Result::Ok),
      static_cast<int>(bitmap16::SketchCodec::encode(
          original, bytes, sizeof(bytes), written)));
  TEST_ASSERT_EQUAL_UINT32(
      bitmap16::SketchCodec::kCurrentFileSize,
      written);
  TEST_ASSERT_EQUAL_UINT8(bitmap16::SketchCodec::kCurrentVersion, bytes[0]);

  Sketch decoded;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::SketchCodec::Result::Ok),
      static_cast<int>(
          bitmap16::SketchCodec::decode(bytes, written, decoded)));
  TEST_ASSERT_EQUAL_UINT8(16, decoded.gridSize);
  TEST_ASSERT_EQUAL_UINT8(8, decoded.paletteSize);
  TEST_ASSERT_EQUAL_HEX16(original.paletteColors[15], decoded.paletteColors[15]);
  TEST_ASSERT_EQUAL_UINT8(1, decoded.pixels[0][0]);
  TEST_ASSERT_EQUAL_UINT8(8, decoded.pixels[15][15]);
  TEST_ASSERT_FALSE(decoded.isEmpty);
}

void test_codec_reads_legacy_v1_format() {
  Sketch original = makeSketch(8, 4);
  original.pixels[4][5] = 4;

  uint8_t v2[bitmap16::SketchCodec::kCurrentFileSize] = {};
  std::size_t written = 0;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::SketchCodec::Result::Ok),
      static_cast<int>(bitmap16::SketchCodec::encode(
          original, v2, sizeof(v2), written)));

  Sketch decoded;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::SketchCodec::Result::Ok),
      static_cast<int>(bitmap16::SketchCodec::decode(
          v2 + 1,
          bitmap16::SketchCodec::kLegacyFileSize,
          decoded)));
  TEST_ASSERT_EQUAL_UINT8(8, decoded.gridSize);
  TEST_ASSERT_EQUAL_UINT8(4, decoded.paletteSize);
  TEST_ASSERT_EQUAL_UINT8(4, decoded.pixels[4][5]);
}

void test_codec_rejects_wrong_version_and_size() {
  uint8_t bytes[bitmap16::SketchCodec::kCurrentFileSize] = {};
  bytes[0] = 99;
  Sketch sketch;
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::SketchCodec::Result::UnsupportedVersion),
      static_cast<int>(
          bitmap16::SketchCodec::decode(bytes, sizeof(bytes), sketch)));

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::SketchCodec::Result::InvalidSize),
      static_cast<int>(bitmap16::SketchCodec::decode(bytes, 20, sketch)));
}

void test_led_mapping_matches_current_single_and_quad_layout() {
  TEST_ASSERT_EQUAL_UINT16(0, bitmap16::LedMapping::indexFor(0, 0, 1, 0));
  TEST_ASSERT_EQUAL_UINT16(63, bitmap16::LedMapping::indexFor(7, 7, 1, 0));
  TEST_ASSERT_EQUAL_UINT16(7, bitmap16::LedMapping::indexFor(0, 0, 1, 1));

  TEST_ASSERT_EQUAL_UINT16(63, bitmap16::LedMapping::indexFor(0, 0, 4, 0));
  TEST_ASSERT_EQUAL_UINT16(64, bitmap16::LedMapping::indexFor(8, 0, 4, 0));
  TEST_ASSERT_EQUAL_UINT16(128, bitmap16::LedMapping::indexFor(8, 8, 4, 0));
  TEST_ASSERT_EQUAL_UINT16(255, bitmap16::LedMapping::indexFor(0, 8, 4, 0));
}

void test_rgb565_expansion_reaches_channel_extremes() {
  const bitmap16::LedMapping::Rgb888 white =
      bitmap16::LedMapping::rgb565ToRgb888(0xffff);
  TEST_ASSERT_EQUAL_UINT8(255, white.red);
  TEST_ASSERT_EQUAL_UINT8(255, white.green);
  TEST_ASSERT_EQUAL_UINT8(255, white.blue);
}

void test_led_mapping_is_bijective_for_every_rotation() {
  const uint8_t unitOptions[] = {1, 4};
  for (uint8_t units : unitOptions) {
    const uint8_t size = units == 1 ? 8 : 16;
    const uint16_t ledCount = units == 1 ? 64 : 256;
    for (uint8_t rotation = 0; rotation < 4; ++rotation) {
      bool seen[256] = {};
      for (uint8_t y = 0; y < size; ++y) {
        for (uint8_t x = 0; x < size; ++x) {
          const uint16_t index =
              bitmap16::LedMapping::indexFor(x, y, units, rotation);
          TEST_ASSERT_LESS_THAN_UINT16(ledCount, index);
          TEST_ASSERT_FALSE(seen[index]);
          seen[index] = true;
        }
      }
      for (uint16_t index = 0; index < ledCount; ++index) {
        TEST_ASSERT_TRUE(seen[index]);
      }
    }
  }
}

void test_led_mapping_rotates_single_matrix_corners() {
  TEST_ASSERT_EQUAL_UINT16(
      7, bitmap16::LedMapping::indexFor(0, 0, 1, 1));
  TEST_ASSERT_EQUAL_UINT16(
      63, bitmap16::LedMapping::indexFor(0, 0, 1, 2));
  TEST_ASSERT_EQUAL_UINT16(
      56, bitmap16::LedMapping::indexFor(0, 0, 1, 3));
}

void test_clock_elapsed_handles_uint32_wraparound() {
  TEST_ASSERT_EQUAL_UINT32(6, bitmap16::Clock::elapsed(3, 0xfffffffd));
  TEST_ASSERT_TRUE(bitmap16::Clock::hasElapsed(3, 0xfffffffd, 6));
}

void test_input_arrow_edges_and_repeat_timing() {
  bitmap16::InputProcessor processor;
  bitmap16::RawInputState right = rawKeys("/");

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Right),
      static_cast<int>(processor.process(right, 1000).event));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::None),
      static_cast<int>(processor.process(right, 1299).event));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Right),
      static_cast<int>(processor.process(right, 1300).event));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::None),
      static_cast<int>(processor.process(right, 1399).event));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Right),
      static_cast<int>(processor.process(right, 1400).event));
}

void test_input_repeat_timing_handles_clock_rollover() {
  bitmap16::InputProcessor processor;
  bitmap16::RawInputState up = rawKeys(";");

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Up),
      static_cast<int>(processor.process(up, 0xfffffff0).event));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Up),
      static_cast<int>(processor.process(up, 0x0000011c).event));
}

void test_input_preserves_draw_while_moving_chord() {
  bitmap16::InputProcessor processor;
  bitmap16::RawInputState raw = rawKeys("m/");
  raw.enter = true;

  const bitmap16::InputFrame frame = processor.process(raw, 10);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Right),
      static_cast<int>(frame.event));
  TEST_ASSERT_TRUE(frame.rightHeld);
  TEST_ASSERT_TRUE(frame.mHeld);
  TEST_ASSERT_TRUE(frame.enterHeld);
  TEST_ASSERT_TRUE(frame.enterPressed);
}

void test_input_exposes_brightness_and_led_chords() {
  bitmap16::InputProcessor processor;
  bitmap16::RawInputState brightness = rawKeys("b=");
  bitmap16::InputFrame frame = processor.process(brightness, 10);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Plus),
      static_cast<int>(frame.event));
  TEST_ASSERT_TRUE(frame.bHeld);

  processor.reset();
  bitmap16::RawInputState led = rawKeys("l");
  led.enter = true;
  frame = processor.process(led, 20);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Enter),
      static_cast<int>(frame.event));
  TEST_ASSERT_TRUE(frame.lHeld);
  TEST_ASSERT_TRUE(frame.enterPressed);
}

void test_input_enter_delete_and_action_are_edge_triggered() {
  bitmap16::InputProcessor processor;
  bitmap16::RawInputState raw;
  raw.enter = true;
  raw.deleteKey = true;
  raw.actionButton = true;

  bitmap16::InputFrame frame = processor.process(raw, 0);
  TEST_ASSERT_TRUE(frame.enterPressed);
  TEST_ASSERT_TRUE(frame.deletePressed);
  TEST_ASSERT_TRUE(frame.actionPressed);

  frame = processor.process(raw, 1);
  TEST_ASSERT_FALSE(frame.enterPressed);
  TEST_ASSERT_FALSE(frame.deletePressed);
  TEST_ASSERT_FALSE(frame.actionPressed);

  processor.process(bitmap16::RawInputState{}, 2);
  frame = processor.process(raw, 3);
  TEST_ASSERT_TRUE(frame.enterPressed);
  TEST_ASSERT_TRUE(frame.deletePressed);
  TEST_ASSERT_TRUE(frame.actionPressed);
}

void test_input_reports_keyboard_held_for_unmapped_keys() {
  bitmap16::InputProcessor processor;
  bitmap16::RawInputState raw;
  raw.keyPressed = true;

  bitmap16::InputFrame frame = processor.process(raw, 0);
  TEST_ASSERT_TRUE(frame.keyboardHeld);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::None),
      static_cast<int>(frame.event));

  frame = processor.process(bitmap16::RawInputState{}, 1);
  TEST_ASSERT_FALSE(frame.keyboardHeld);

  bitmap16::RawInputState enterOnly;
  enterOnly.enter = true;
  frame = processor.process(enterOnly, 2);
  TEST_ASSERT_TRUE(frame.keyboardHeld);
}

void test_input_maps_numbers_and_command_characters() {
  bitmap16::InputProcessor processor;
  bitmap16::InputFrame frame = processor.process(rawKeys("7"), 0);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Number7),
      static_cast<int>(frame.event));

  processor.reset();
  frame = processor.process(rawKeys("P"), 1);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Character),
      static_cast<int>(frame.event));
  TEST_ASSERT_EQUAL_CHAR('P', frame.character);
}

void test_input_maps_preview_and_palette_controls() {
  bitmap16::InputProcessor processor;

  bitmap16::InputFrame frame = processor.process(rawKeys(" "), 0);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Space),
      static_cast<int>(frame.event));

  frame = processor.process(rawKeys("`"), 1);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Escape),
      static_cast<int>(frame.event));

  frame = processor.process(rawKeys("4"), 2);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Number4),
      static_cast<int>(frame.event));

  frame = processor.process(rawKeys("U"), 3);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Character),
      static_cast<int>(frame.event));
  TEST_ASSERT_EQUAL_CHAR('U', frame.character);
}

void test_input_preserves_settings_activation_modifiers() {
  bitmap16::InputProcessor processor;
  bitmap16::RawInputState raw = rawKeys(",");
  raw.fn = true;

  bitmap16::InputFrame frame = processor.process(raw, 0);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Left),
      static_cast<int>(frame.event));
  TEST_ASSERT_TRUE(frame.fnHeld);
  TEST_ASSERT_TRUE(frame.keyboardHeld);

  processor.reset();
  frame = processor.process(rawKeys(" "), 1);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Space),
      static_cast<int>(frame.event));
}

void test_input_preserves_canvas_command_chords() {
  bitmap16::InputProcessor processor;

  bitmap16::RawInputState highColor = rawKeys("8");
  highColor.fn = true;
  bitmap16::InputFrame frame = processor.process(highColor, 0);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Number8),
      static_cast<int>(frame.event));
  TEST_ASSERT_TRUE(frame.fnHeld);

  processor.reset();
  bitmap16::RawInputState charging = rawKeys("b");
  charging.fn = true;
  frame = processor.process(charging, 1);
  TEST_ASSERT_TRUE(frame.bHeld);
  TEST_ASSERT_TRUE(frame.fnHeld);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::None),
      static_cast<int>(frame.event));

  processor.reset();
  bitmap16::RawInputState eraseMove = rawKeys("/");
  eraseMove.deleteKey = true;
  frame = processor.process(eraseMove, 2);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::InputEvent::Right),
      static_cast<int>(frame.event));
  TEST_ASSERT_TRUE(frame.deleteHeld);
  TEST_ASSERT_TRUE(frame.deletePressed);
}

void test_shake_detector_uses_threshold_and_initial_cooldown() {
  bitmap16::ShakeDetector detector;
  bitmap16::AccelerationSample strong = {6.1f, 0.0f, 0.0f, true};
  bitmap16::AccelerationSample boundary = {6.0f, 0.0f, 0.0f, true};

  TEST_ASSERT_FALSE(detector.update(strong, 499));
  TEST_ASSERT_FALSE(detector.update(boundary, 500));
  TEST_ASSERT_TRUE(detector.update(strong, 500));
}

void test_shake_detector_rejects_unavailable_and_normal_gravity() {
  bitmap16::ShakeDetector detector;
  bitmap16::AccelerationSample unavailable = {10.0f, 0.0f, 0.0f, false};
  bitmap16::AccelerationSample gravity = {0.0f, 0.0f, 1.0f, true};

  TEST_ASSERT_FALSE(detector.update(unavailable, 1000));
  TEST_ASSERT_FALSE(detector.update(gravity, 1000));
}

void test_shake_detector_enforces_cooldown_and_handles_rollover() {
  bitmap16::ShakeDetector detector;
  bitmap16::AccelerationSample strong = {4.0f, 4.0f, 4.0f, true};

  TEST_ASSERT_TRUE(detector.update(strong, 0xfffffff0));
  TEST_ASSERT_FALSE(detector.update(strong, 0x00000100));
  TEST_ASSERT_TRUE(detector.update(strong, 0x000001e4));
}

void test_app_requires_a_frame_callback() {
  bitmap16::App app;

  app.tick();
  TEST_ASSERT_FALSE(app.isInitialized());
  TEST_ASSERT_EQUAL_UINT32(0, app.frameCount());
  TEST_ASSERT_FALSE(app.init(nullptr));
  TEST_ASSERT_FALSE(app.isInitialized());
}

void test_app_ticks_initialized_runtime() {
  bitmap16::App app;
  appFrameCalls = 0;

  TEST_ASSERT_TRUE(app.init(countAppFrame));
  app.tick();
  app.tick();

  TEST_ASSERT_TRUE(app.isInitialized());
  TEST_ASSERT_EQUAL_INT(2, appFrameCalls);
  TEST_ASSERT_EQUAL_UINT32(2, app.frameCount());
}

void test_app_tracks_view_transitions() {
  bitmap16::App app;
  app.init(countAppFrame);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ViewId::Canvas),
      static_cast<int>(app.currentView()));
  app.setView(bitmap16::ViewId::Palette);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ViewId::Palette),
      static_cast<int>(app.currentView()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ViewId::Canvas),
      static_cast<int>(app.previousView()));

  app.setView(bitmap16::ViewId::Palette);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ViewId::Canvas),
      static_cast<int>(app.previousView()));
  app.setView(bitmap16::ViewId::Settings);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ViewId::Palette),
      static_cast<int>(app.previousView()));
}

void test_app_view_history_supports_help_return_navigation() {
  bitmap16::App app;
  app.init(countAppFrame);

  app.setView(bitmap16::ViewId::Memory);
  app.setView(bitmap16::ViewId::Help);
  const bitmap16::ViewId returnView = app.previousView();
  app.setView(returnView);

  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ViewId::Memory),
      static_cast<int>(app.currentView()));
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ViewId::Help),
      static_cast<int>(app.previousView()));
}

void test_settings_normalization_preserves_valid_values() {
  bitmap16::Settings settings;
  settings.theme = bitmap16::ThemeId::Dark;
  settings.defaultGridSize = 16;
  settings.matrixUnits = 4;
  settings.matrixRotation = 3;
  settings.exportFormat = bitmap16::ExportFormat::Rgb565;
  settings.shakeUndoEnabled = true;
  settings.matrixEnabled = true;
  settings.displayBrightness = 70;
  settings.matrixBrightness = 12;

  const bitmap16::Settings normalized =
      bitmap16::normalizeSettings(settings);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ThemeId::Dark),
      static_cast<int>(normalized.theme));
  TEST_ASSERT_EQUAL_UINT8(16, normalized.defaultGridSize);
  TEST_ASSERT_EQUAL_UINT8(4, normalized.matrixUnits);
  TEST_ASSERT_EQUAL_UINT8(3, normalized.matrixRotation);
  TEST_ASSERT_TRUE(normalized.matrixEnabled);
  TEST_ASSERT_EQUAL_UINT8(70, normalized.displayBrightness);
  TEST_ASSERT_EQUAL_UINT8(12, normalized.matrixBrightness);
}

void test_settings_normalization_repairs_invalid_values() {
  bitmap16::Settings settings;
  settings.theme = static_cast<bitmap16::ThemeId>(99);
  settings.defaultGridSize = 12;
  settings.matrixUnits = 2;
  settings.matrixRotation = 7;
  settings.exportFormat = static_cast<bitmap16::ExportFormat>(99);
  settings.displayBrightness = 0;
  settings.matrixBrightness = 100;

  const bitmap16::Settings normalized =
      bitmap16::normalizeSettings(settings);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ThemeId::Light),
      static_cast<int>(normalized.theme));
  TEST_ASSERT_EQUAL_UINT8(8, normalized.defaultGridSize);
  TEST_ASSERT_EQUAL_UINT8(1, normalized.matrixUnits);
  TEST_ASSERT_EQUAL_UINT8(3, normalized.matrixRotation);
  TEST_ASSERT_EQUAL_INT(
      static_cast<int>(bitmap16::ExportFormat::Rgb888),
      static_cast<int>(normalized.exportFormat));
  TEST_ASSERT_EQUAL_UINT8(10, normalized.displayBrightness);
  TEST_ASSERT_EQUAL_UINT8(20, normalized.matrixBrightness);
}

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_editor_draw_erase_and_undo);
  RUN_TEST(test_canvas_allocation_fill_and_release);
  RUN_TEST(test_canvas_clips_rectangles_and_pixels);
  RUN_TEST(test_canvas_view_layout_is_resolution_and_grid_aware);
  RUN_TEST(test_canvas_view_renders_editor_at_both_target_sizes);
  RUN_TEST(test_canvas_draws_lines_and_rectangles);
  RUN_TEST(test_canvas_pushes_clipped_and_byte_swapped_images);
  RUN_TEST(test_canvas_draws_scaled_and_aligned_text);
  RUN_TEST(test_help_view_navigation_clamps_to_available_items);
  RUN_TEST(test_help_view_renders_and_scrolls_at_both_target_sizes);
  RUN_TEST(test_settings_view_navigation_and_actions_are_portable);
  RUN_TEST(test_settings_view_renders_at_both_target_sizes);
  RUN_TEST(test_preview_view_background_selection_is_bounded);
  RUN_TEST(test_preview_view_renders_indexed_pixels_at_both_target_sizes);
  RUN_TEST(test_charging_view_initializes_and_bounces_within_bounds);
  RUN_TEST(
      test_charging_view_renders_icons_sketch_and_battery_at_target_sizes);
  RUN_TEST(test_palette_view_filters_navigates_and_animates_selection);
  RUN_TEST(test_palette_view_renders_carousel_at_both_target_sizes);
  RUN_TEST(test_memory_view_navigation_and_scroll_are_resolution_aware);
  RUN_TEST(test_memory_view_renders_new_and_sketch_tiles_at_target_sizes);
  RUN_TEST(test_editor_flood_fill_is_four_way_and_bounded);
  RUN_TEST(test_editor_shift_wraps_active_grid);
  RUN_TEST(test_editor_repeated_shift_preserves_one_undo_snapshot);
  RUN_TEST(test_editor_toggle_grid_clamps_cursor_and_undo_restores_grid);
  RUN_TEST(test_palette_index_collapse_matches_existing_rules);
  RUN_TEST(test_sketch_initialization_repeats_small_palettes);
  RUN_TEST(test_palette_parser_accepts_lospec_format_and_repeats_colors);
  RUN_TEST(test_palette_parser_rejects_unsupported_count);
  RUN_TEST(test_codec_v2_round_trip_is_exact);
  RUN_TEST(test_codec_reads_legacy_v1_format);
  RUN_TEST(test_codec_rejects_wrong_version_and_size);
  RUN_TEST(test_led_mapping_matches_current_single_and_quad_layout);
  RUN_TEST(test_rgb565_expansion_reaches_channel_extremes);
  RUN_TEST(test_led_mapping_is_bijective_for_every_rotation);
  RUN_TEST(test_led_mapping_rotates_single_matrix_corners);
  RUN_TEST(test_clock_elapsed_handles_uint32_wraparound);
  RUN_TEST(test_input_arrow_edges_and_repeat_timing);
  RUN_TEST(test_input_repeat_timing_handles_clock_rollover);
  RUN_TEST(test_input_preserves_draw_while_moving_chord);
  RUN_TEST(test_input_exposes_brightness_and_led_chords);
  RUN_TEST(test_input_enter_delete_and_action_are_edge_triggered);
  RUN_TEST(test_input_reports_keyboard_held_for_unmapped_keys);
  RUN_TEST(test_input_maps_numbers_and_command_characters);
  RUN_TEST(test_input_maps_preview_and_palette_controls);
  RUN_TEST(test_input_preserves_settings_activation_modifiers);
  RUN_TEST(test_input_preserves_canvas_command_chords);
  RUN_TEST(test_shake_detector_uses_threshold_and_initial_cooldown);
  RUN_TEST(test_shake_detector_rejects_unavailable_and_normal_gravity);
  RUN_TEST(test_shake_detector_enforces_cooldown_and_handles_rollover);
  RUN_TEST(test_app_requires_a_frame_callback);
  RUN_TEST(test_app_ticks_initialized_runtime);
  RUN_TEST(test_app_tracks_view_transitions);
  RUN_TEST(test_app_view_history_supports_help_return_navigation);
  RUN_TEST(test_settings_normalization_preserves_valid_values);
  RUN_TEST(test_settings_normalization_repairs_invalid_values);
  return UNITY_END();
}
