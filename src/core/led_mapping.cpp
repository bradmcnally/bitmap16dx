#include "core/led_mapping.h"

namespace bitmap16 {
namespace LedMapping {

uint16_t indexFor(
    uint8_t x,
    uint8_t y,
    uint8_t matrixUnits,
    uint8_t rotation) {
  const uint8_t maxIndex = matrixUnits == 1 ? 7 : 15;
  uint8_t adjustedX = x;
  uint8_t adjustedY = y;

  switch (rotation % 4) {
    case 1:
      adjustedX = static_cast<uint8_t>(maxIndex - y);
      adjustedY = x;
      break;
    case 2:
      adjustedX = static_cast<uint8_t>(maxIndex - x);
      adjustedY = static_cast<uint8_t>(maxIndex - y);
      break;
    case 3:
      adjustedX = y;
      adjustedY = static_cast<uint8_t>(maxIndex - x);
      break;
    default:
      break;
  }

  if (matrixUnits == 1) {
    return static_cast<uint16_t>(adjustedY * 8 + adjustedX);
  }

  const uint8_t unitX = adjustedX / 8;
  const uint8_t unitY = adjustedY / 8;
  const uint8_t localX = adjustedX % 8;
  const uint8_t localY = adjustedY % 8;
  const uint8_t unit =
      unitY == 0 ? unitX : static_cast<uint8_t>(unitX == 0 ? 3 : 2);

  uint8_t rotatedX = localX;
  uint8_t rotatedY = localY;
  if (unit == 0 || unit == 3) {
    rotatedX = static_cast<uint8_t>(7 - localX);
    rotatedY = static_cast<uint8_t>(7 - localY);
  }

  return static_cast<uint16_t>(
      unit * 64 + rotatedY * 8 + rotatedX);
}

Rgb888 rgb565ToRgb888(uint16_t color) {
  return {
      static_cast<uint8_t>(((color >> 11) & 0x1f) * 255 / 31),
      static_cast<uint8_t>(((color >> 5) & 0x3f) * 255 / 63),
      static_cast<uint8_t>((color & 0x1f) * 255 / 31),
  };
}

}  // namespace LedMapping
}  // namespace bitmap16
