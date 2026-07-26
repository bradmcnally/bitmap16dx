#include "core/sketch_codec.h"

namespace bitmap16 {
namespace SketchCodec {

Result encode(
    const Sketch& sketch,
    uint8_t* output,
    std::size_t outputCapacity,
    std::size_t& bytesWritten) {
  bytesWritten = 0;
  if (output == nullptr || outputCapacity < kCurrentFileSize) {
    return Result::BufferTooSmall;
  }
  if (!isSupportedGridSize(sketch.gridSize) ||
      !isSupportedPaletteSize(sketch.paletteSize)) {
    return Result::InvalidData;
  }

  std::size_t offset = 0;
  output[offset++] = kCurrentVersion;
  output[offset++] = sketch.gridSize;
  output[offset++] = sketch.paletteSize;

  for (std::size_t i = 0; i < kMaxPaletteColors; ++i) {
    output[offset++] = static_cast<uint8_t>(sketch.paletteColors[i] >> 8);
    output[offset++] = static_cast<uint8_t>(sketch.paletteColors[i] & 0xff);
  }
  for (std::size_t y = 0; y < kMaxGridSize; ++y) {
    for (std::size_t x = 0; x < kMaxGridSize; ++x) {
      output[offset++] = sketch.pixels[y][x];
    }
  }

  bytesWritten = offset;
  return Result::Ok;
}

Result decode(const uint8_t* data, std::size_t size, Sketch& sketch) {
  if (data == nullptr) {
    return Result::InvalidData;
  }
  if (size != kLegacyFileSize &&
      size != kVersion2FileSize &&
      size != kCurrentFileSize) {
    return Result::InvalidSize;
  }

  std::size_t offset = 0;
  std::size_t storedGridCapacity = 16;
  if (size != kLegacyFileSize) {
    const uint8_t version = data[offset++];
    if (version == 2 && size == kVersion2FileSize) {
      storedGridCapacity = 16;
    } else if (version == kCurrentVersion && size == kCurrentFileSize) {
      storedGridCapacity = kMaxGridSize;
    } else {
      return Result::UnsupportedVersion;
    }
  }

  Sketch decoded;
  decoded.gridSize = data[offset++];
  decoded.paletteSize = data[offset++];
  if (!isSupportedGridSize(decoded.gridSize) ||
      !isSupportedPaletteSize(decoded.paletteSize)) {
    return Result::InvalidData;
  }

  for (std::size_t i = 0; i < kMaxPaletteColors; ++i) {
    const uint16_t high = data[offset++];
    const uint16_t low = data[offset++];
    decoded.paletteColors[i] = static_cast<uint16_t>((high << 8) | low);
  }
  for (std::size_t y = 0; y < storedGridCapacity; ++y) {
    for (std::size_t x = 0; x < storedGridCapacity; ++x) {
      const uint8_t pixel = data[offset++];
      if (pixel > kMaxPaletteColors) {
        return Result::InvalidData;
      }
      decoded.pixels[y][x] = pixel;
    }
  }

  decoded.isEmpty = true;
  for (std::size_t y = 0; y < kMaxGridSize && decoded.isEmpty; ++y) {
    for (std::size_t x = 0; x < kMaxGridSize; ++x) {
      if (decoded.pixels[y][x] != 0) {
        decoded.isEmpty = false;
        break;
      }
    }
  }

  sketch = decoded;
  return Result::Ok;
}

}  // namespace SketchCodec
}  // namespace bitmap16
