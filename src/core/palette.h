#pragma once

#include <cstddef>
#include <cstdint>

namespace bitmap16 {
namespace Palette {

constexpr std::size_t kColorCount = 16;

struct Parsed {
  uint16_t colors[kColorCount] = {};
  uint8_t size = 0;
};

uint8_t collapseIndex(uint8_t index, uint8_t paletteSize);
uint16_t colorForIndex(
    const uint16_t* colors,
    uint8_t paletteSize,
    uint8_t pixelIndex);
uint16_t rgb888ToRgb565(uint8_t red, uint8_t green, uint8_t blue);
bool parseLospecHex(const char* text, std::size_t length, Parsed& parsed);

}  // namespace Palette
}  // namespace bitmap16
