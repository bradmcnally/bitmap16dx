#include "core/help_view.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void require(bool condition, const char* message) {
  if (!condition) { std::cerr << message << '\n'; std::exit(1); }
}

void capture(const bitmap16::Canvas& canvas, int group) {
  const char* directory = std::getenv("BITMAP16_HELP_CAPTURES");
  if (!directory) return;
  std::ofstream image(std::string(directory) + "/help-" + std::to_string(group) + ".ppm",
                      std::ios::binary);
  image << "P6\n320 200\n255\n";
  for (int i = 0; i < canvas.width() * canvas.height(); ++i) {
    const auto pixel = canvas.pixels()[i];
    const char rgb[] = {static_cast<char>(((pixel >> 11) & 31) * 255 / 31),
                        static_cast<char>(((pixel >> 5) & 63) * 255 / 63),
                        static_cast<char>((pixel & 31) * 255 / 31)};
    image.write(rgb, sizeof(rgb));
  }
}
}  // namespace

int main() {
  using namespace bitmap16;
  Canvas canvas, expected;
  require(canvas.create(320, 200) && expected.create(320, 200), "Create Help frames");
  HelpView::Theme theme;
  HelpView::State state;
  constexpr int starts[] = {0, 7, 9, 19, 25, 29, 33};
  constexpr const char* names[] = {"DRAW", "CANVAS", "PALETTES", "SKETCHES", "PREVIEW", "APP", "KEYBOARD"};
  require(HelpView::itemCount(false, false) == 40, "All Deck controls are visible");
  require(!HelpView::moveGroup(state, -1, false, false), "First section boundary");
  for (int group = 0; group < 7; ++group) {
    require(state.cursor == starts[group], "Section navigation lands on its first control");
    HelpView::render(canvas, state, theme, false, false);
    capture(canvas, group);
    expected.fillScreen(theme.background);
    expected.setTextSize(1);
    expected.setTextColor(theme.text);
    expected.setTextAlign(TextAlign::Left);
    expected.drawString("HELP > ", 4, 4);
    expected.drawString(names[group], 46, 4);
    expected.setTextColor(theme.textSecondary);
    expected.drawString("LB/RB:Sections", 3, 189);
    expected.setTextAlign(TextAlign::Right);
    expected.drawString("B:Back", 317, 189);
    for (int y = 0; y < 200; ++y) {
      if (y >= 18 && y < 186) continue;
      for (int x = 0; x < 320; ++x)
        require(canvas.readPixel(x, y) == expected.readPixel(x, y), "Header and footer stay clear of scrolling controls");
    }
    if (group == 0) {
      for (int y = 126; y < 186; ++y)
        for (int x = 0; x < 320; ++x)
          require(canvas.readPixel(x, y) == theme.background, "Help renders one task section at a time");
      HelpView::moveCursor(state, 1, false, false);
      HelpView::render(canvas, state, theme, false, false, true);
      expected.fillScreen(theme.background);
      expected.setTextAlign(TextAlign::Left);
      expected.setTextColor(theme.text);
      expected.setTextSize(2);
      expected.drawString("A", 192, 36);
      for (int y = 36; y < 50; ++y)
        for (int x = 192; x < 280; ++x)
          require(canvas.readPixel(x, y) == expected.readPixel(x, y), "Draw uses the Deck A button");
    }
    if (group < 6) require(HelpView::moveGroup(state, 1, false, false), "Next section");
  }
  require(!HelpView::moveGroup(state, 1, false, false), "Last section boundary");
  HelpView::moveCursor(state, 100, false, false);
  HelpView::render(canvas, state, theme, false, false);
  Canvas shortCanvas;
  require(shortCanvas.create(320, 100), "Create a short Help viewport");
  HelpView::render(shortCanvas, state, theme, false, false);
  require(state.scrollOffset > starts[6], "Short viewports scroll to the selected control");
  for (int group = 5; group >= 0; --group) {
    require(HelpView::moveGroup(state, -1, false, false) && state.cursor == starts[group], "Previous section");
  }
}
