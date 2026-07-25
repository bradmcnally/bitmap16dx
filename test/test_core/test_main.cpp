#include <unity.h>

#include <cstring>

#include "core/app.h"
#include "core/clock.h"
#include "core/canvas.h"
#include "core/editor.h"
#include "core/input.h"
#include "core/led_mapping.h"
#include "core/palette.h"
#include "core/shake_detector.h"
#include "core/sketch.h"
#include "core/sketch_codec.h"

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

  editor.draw();
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

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_editor_draw_erase_and_undo);
  RUN_TEST(test_canvas_allocation_fill_and_release);
  RUN_TEST(test_canvas_clips_rectangles_and_pixels);
  RUN_TEST(test_canvas_draws_lines_and_rectangles);
  RUN_TEST(test_canvas_pushes_clipped_and_byte_swapped_images);
  RUN_TEST(test_canvas_draws_scaled_and_aligned_text);
  RUN_TEST(test_editor_flood_fill_is_four_way_and_bounded);
  RUN_TEST(test_editor_shift_wraps_active_grid);
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
  return UNITY_END();
}
