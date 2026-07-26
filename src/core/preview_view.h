#pragma once

#include <cstdint>

#include "core/canvas.h"
#include "core/sketch.h"

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
  // Zero means fit to the current preview viewport.
  uint8_t zoom = 0;
};

struct Theme {
  uint16_t black;
  uint16_t white;
  uint16_t gray;
  uint16_t dark;
};

struct Image {
  const uint8_t (*pixels)[kMaxGridSize] = nullptr;
  uint8_t gridSize = 8;
  const uint16_t* paletteColors = nullptr;
  uint8_t paletteSize = 16;
};

bool selectBackground(State& state, int selection);
bool adjustZoom(
    State& state,
    int delta,
    uint8_t gridSize,
    int availableSize);
int resolvedZoom(
    const State& state,
    uint8_t gridSize,
    int availableSize);
uint16_t backgroundColor(const State& state, const Theme& theme);
void render(
    Canvas& canvas,
    const State& state,
    const Image& image,
    const Theme& theme,
    const char* statusMessage = nullptr,
    bool statusCentered = true);

}  // namespace PreviewView
}  // namespace bitmap16
