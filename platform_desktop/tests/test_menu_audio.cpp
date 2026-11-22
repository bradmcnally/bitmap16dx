#include "menu_audio.h"

#include <cstdlib>
#include <iostream>

namespace {
void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
}

int main() {
  using namespace bitmap16::Desktop;
  for (const auto menu : {View::Palette, View::Memory, View::Settings, View::Help}) {
    require(menuTransitionCue(View::Canvas, menu, View::Canvas) == MenuCue::Open,
            "Opening each requested menu plays enable");
    require(menuTransitionCue(menu, View::Canvas, View::Canvas) == MenuCue::Close,
            "Closing each requested menu plays disable");
    require(menuTransitionCue(menu, menu, View::Canvas) == MenuCue::None,
            "Menu redraws, cursor movement, and list reloads stay silent");
    if (menu != View::Help) {
      require(menuTransitionCue(menu, View::Help, menu) == MenuCue::Open,
              "Opening Help over a menu plays enable");
      require(menuTransitionCue(View::Help, menu, menu) == MenuCue::Close,
              "Returning from Help plays disable without replaying parent enable");
    }
  }
  require(menuTransitionCue(View::Palette, View::Canvas, View::Canvas, true) == MenuCue::None,
          "Successful cartridge insertion keeps its dedicated success sound");
  require(menuTransitionCue(View::Memory, View::Canvas, View::Canvas, false, true) == MenuCue::SketchOpen,
          "Opening an existing sketch replaces the dismiss cue with its own sound");
  require(menuTransitionCue(View::Memory, View::Memory, View::Canvas) == MenuCue::None,
          "Canceled or failed sketch opens leave Sketches visible without an open cue");
  require(menuTransitionCue(View::Canvas, View::Preview, View::Canvas) == MenuCue::None &&
          menuTransitionCue(View::Preview, View::Canvas, View::Canvas) == MenuCue::None,
          "Preview transitions do not gain unrelated menu sounds");
}
