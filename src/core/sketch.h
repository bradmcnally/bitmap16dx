#pragma once

#include <cstddef>
#include <cstdint>

namespace bitmap16 {

constexpr std::size_t kMaxGridSize = 16;
constexpr std::size_t kMaxPaletteColors = 16;

struct Sketch {
  uint8_t pixels[kMaxGridSize][kMaxGridSize] = {};
  uint8_t gridSize = 8;
  uint8_t paletteSize = 16;
  uint16_t paletteColors[kMaxPaletteColors] = {};
  bool isEmpty = true;
};

bool isSupportedGridSize(uint8_t gridSize);
bool isSupportedPaletteSize(uint8_t paletteSize);
void initializeSketch(
    Sketch& sketch,
    uint8_t gridSize,
    const uint16_t* paletteColors,
    uint8_t paletteSize);

}  // namespace bitmap16
