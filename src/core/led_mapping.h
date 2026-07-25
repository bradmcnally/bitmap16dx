#pragma once

#include <cstdint>

namespace bitmap16 {
namespace LedMapping {

struct Rgb888 {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};

uint16_t indexFor(uint8_t x, uint8_t y, uint8_t matrixUnits, uint8_t rotation);
Rgb888 rgb565ToRgb888(uint16_t color);

}  // namespace LedMapping
}  // namespace bitmap16
