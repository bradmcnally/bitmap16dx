#include "core/help_view.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

void capture(const bitmap16::Canvas& canvas, bool external) {
  const char* directory = std::getenv("BITMAP16_HELP_CAPTURES");
  if (!directory) return;
  std::ofstream file(std::string(directory) + (external ? "/usb.ppm" : "/built-in.ppm"),
                     std::ios::binary);
  file << "P6\n320 170\n255\n";
  for (int i = 0; i < canvas.width() * canvas.height(); ++i) {
    const auto pixel = canvas.pixels()[i];
    const char rgb[] = {static_cast<char>(((pixel >> 11) & 31) * 255 / 31),
                        static_cast<char>(((pixel >> 5) & 63) * 255 / 63),
                        static_cast<char>((pixel & 31) * 255 / 31)};
    file.write(rgb, sizeof(rgb));
  }
}
}

int main() {
  using namespace bitmap16;
  Canvas canvas, expected;
  require(canvas.create(320, 170) && expected.create(320, 170), "Create Zero Help frames");
  HelpView::Theme theme;
  struct Control { int index; const char* builtIn; const char* usb; };
  constexpr Control controls[] = {
      {0, "F Z X C", "Arrows"}, {1, "Enter/Spc", "Enter/Spc"},
      {2, "Backspace", "Bksp/Del"}, {5, "M + FZXC", "M arrows"},
      {7, "Ctrl Y", "Ctrl Sh Z"}, {8, "Fn Bksp", "Built-in"},
      {9, "Fn+L/M", "+/-"}, {14, "Fn 1-8", "F1-F8"},
      {25, "Fn U/I", "Built-in"}};
  for (const bool external : {false, true}) {
    for (const auto& control : controls) {
      HelpView::State state;
      state.cursor = state.scrollOffset = control.index;
      state.externalKeyboard = external;
      HelpView::render(canvas, state, theme, false, true);
      expected.fillScreen(theme.background);
      expected.setTextAlign(TextAlign::Left);
      expected.setTextColor(theme.text);
      expected.setTextSize(2);
      const char* label = external ? control.usb : control.builtIn;
      require(std::string(label).size() * 12 <= 320 - 192, "Selected keyboard labels fit the screen");
      expected.drawString(label, 192, 22);
      for (int y = 22; y < 38; ++y)
        for (int x = 192; x < 320; ++x)
          require(canvas.readPixel(x, y) == expected.readPixel(x, y),
                  "Help shows the mappings for the active keyboard");
      if (control.index == 0) {
        capture(canvas, external);
        expected.setTextSize(1);
        expected.drawString(external ? "HELP USB > DRAW" : "HELP > DRAW", 4, 4);
        for (int y = 0; y < 18; ++y)
          for (int x = 0; x < 320; ++x)
            require(canvas.readPixel(x, y) == expected.readPixel(x, y),
                    "Help header identifies the active layout");
      }
    }
  }
}
