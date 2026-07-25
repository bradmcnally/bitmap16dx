#include "core/palette.h"

#include <cctype>

namespace bitmap16 {
namespace Palette {

namespace {

int hexDigit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool parseColor(const char* text, std::size_t length, uint16_t& color) {
  if (length == 7 && text[0] == '#') {
    ++text;
    --length;
  }
  if (length != 6) {
    return false;
  }

  uint32_t rgb = 0;
  for (std::size_t i = 0; i < length; ++i) {
    const int digit = hexDigit(text[i]);
    if (digit < 0) {
      return false;
    }
    rgb = (rgb << 4) | static_cast<uint32_t>(digit);
  }

  color = rgb888ToRgb565(
      static_cast<uint8_t>(rgb >> 16),
      static_cast<uint8_t>(rgb >> 8),
      static_cast<uint8_t>(rgb));
  return true;
}

}  // namespace

uint8_t collapseIndex(uint8_t index, uint8_t paletteSize) {
  if (index == 0 || index <= paletteSize) {
    return index;
  }
  if (paletteSize == 0) {
    return 0;
  }
  return static_cast<uint8_t>(((index - 1) % paletteSize) + 1);
}

uint16_t colorForIndex(
    const uint16_t* colors,
    uint8_t paletteSize,
    uint8_t pixelIndex) {
  if (colors == nullptr || pixelIndex == 0 || paletteSize == 0) {
    return 0;
  }
  const uint8_t collapsed = collapseIndex(pixelIndex, paletteSize);
  return colors[collapsed - 1];
}

uint16_t rgb888ToRgb565(uint8_t red, uint8_t green, uint8_t blue) {
  return static_cast<uint16_t>(
      ((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
}

bool parseLospecHex(const char* text, std::size_t length, Parsed& parsed) {
  if (text == nullptr) {
    return false;
  }

  Parsed result;
  std::size_t lineStart = 0;
  while (lineStart < length && result.size < kColorCount) {
    std::size_t lineEnd = lineStart;
    while (lineEnd < length && text[lineEnd] != '\n' && text[lineEnd] != '\r') {
      ++lineEnd;
    }

    std::size_t contentStart = lineStart;
    std::size_t contentEnd = lineEnd;
    while (contentStart < contentEnd &&
           std::isspace(static_cast<unsigned char>(text[contentStart]))) {
      ++contentStart;
    }
    while (contentEnd > contentStart &&
           std::isspace(static_cast<unsigned char>(text[contentEnd - 1]))) {
      --contentEnd;
    }

    const bool isComment =
        contentEnd - contentStart >= 2 &&
        text[contentStart] == '/' &&
        text[contentStart + 1] == '/';
    uint16_t color = 0;
    if (!isComment && contentStart < contentEnd &&
        parseColor(
            text + contentStart,
            contentEnd - contentStart,
            color)) {
      result.colors[result.size++] = color;
    }

    lineStart = lineEnd;
    while (lineStart < length &&
           (text[lineStart] == '\n' || text[lineStart] == '\r')) {
      ++lineStart;
    }
  }

  if (result.size != 4 && result.size != 8 && result.size != 16) {
    return false;
  }
  for (std::size_t i = result.size; i < kColorCount; ++i) {
    result.colors[i] = result.colors[i % result.size];
  }
  parsed = result;
  return true;
}

}  // namespace Palette
}  // namespace bitmap16
