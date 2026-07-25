#pragma once

#include <cstdint>

#include "core/canvas.h"

namespace bitmap16 {
namespace PreviewView {

enum class Background : uint8_t {
  Black = 0,
  White = 1,
  Gray = 2,
  Dark = 3,
};

struct State {
  Background background = Background::Black;
};

struct Theme {
  uint16_t black;
  uint16_t white;
  uint16_t gray;
  uint16_t dark;
};

struct Image {
  const uint8_t (*pixels)[16] = nullptr;
  uint8_t gridSize = 8;
  const uint16_t* paletteColors = nullptr;
  uint8_t paletteSize = 16;
};

bool selectBackground(State& state, int selection);
uint16_t backgroundColor(const State& state, const Theme& theme);
void render(
    Canvas& canvas,
    const State& state,
    const Image& image,
    const Theme& theme);

}  // namespace PreviewView
}  // namespace bitmap16
