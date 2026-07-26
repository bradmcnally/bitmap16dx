#include "core/sketch.h"

#include <cstring>

namespace bitmap16 {

bool isSupportedGridSize(uint8_t gridSize) {
  return gridSize == 8 || gridSize == 16 || gridSize == 32;
}

bool isSupportedPaletteSize(uint8_t paletteSize) {
  return paletteSize == 4 || paletteSize == 8 || paletteSize == 16;
}

void initializeSketch(
    Sketch& sketch,
    uint8_t gridSize,
    const uint16_t* paletteColors,
    uint8_t paletteSize) {
  std::memset(sketch.pixels, 0, sizeof(sketch.pixels));
  std::memset(sketch.paletteColors, 0, sizeof(sketch.paletteColors));

  sketch.gridSize = isSupportedGridSize(gridSize) ? gridSize : 8;
  sketch.paletteSize = isSupportedPaletteSize(paletteSize) ? paletteSize : 16;

  if (paletteColors != nullptr) {
    for (std::size_t i = 0; i < sketch.paletteSize; ++i) {
      sketch.paletteColors[i] = paletteColors[i];
    }
    for (std::size_t i = sketch.paletteSize; i < kMaxPaletteColors; ++i) {
      sketch.paletteColors[i] = paletteColors[i % sketch.paletteSize];
    }
  }

  sketch.isEmpty = true;
}

}  // namespace bitmap16
