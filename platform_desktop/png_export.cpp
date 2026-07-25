#include "png_export.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <vector>

namespace bitmap16 {
namespace Desktop {
namespace PngExport {
namespace {

void append32(std::vector<uint8_t>& output, uint32_t value) {
  output.push_back(static_cast<uint8_t>(value >> 24));
  output.push_back(static_cast<uint8_t>(value >> 16));
  output.push_back(static_cast<uint8_t>(value >> 8));
  output.push_back(static_cast<uint8_t>(value));
}

uint32_t crc32(const uint8_t* data, std::size_t size) {
  uint32_t crc = 0xffffffffu;
  for (std::size_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
  }
  return crc ^ 0xffffffffu;
}

uint32_t adler32(const std::vector<uint8_t>& data) {
  uint32_t first = 1;
  uint32_t second = 0;
  for (const uint8_t value : data) {
    first = (first + value) % 65521u;
    second = (second + first) % 65521u;
  }
  return (second << 16) | first;
}

void appendChunk(
    std::vector<uint8_t>& png,
    const std::array<char, 4>& type,
    const std::vector<uint8_t>& data) {
  append32(png, static_cast<uint32_t>(data.size()));
  const std::size_t crcStart = png.size();
  for (const char value : type) {
    png.push_back(static_cast<uint8_t>(value));
  }
  png.insert(png.end(), data.begin(), data.end());
  append32(png, crc32(png.data() + crcStart, png.size() - crcStart));
}

std::vector<uint8_t> deflateStored(const std::vector<uint8_t>& raw) {
  std::vector<uint8_t> output;
  output.reserve(raw.size() + raw.size() / 65535u * 5u + 8u);
  output.push_back(0x78);
  output.push_back(0x01);
  std::size_t offset = 0;
  do {
    const std::size_t count =
        std::min<std::size_t>(65535u, raw.size() - offset);
    const bool final = offset + count == raw.size();
    output.push_back(final ? 1u : 0u);
    output.push_back(static_cast<uint8_t>(count));
    output.push_back(static_cast<uint8_t>(count >> 8));
    const uint16_t inverted =
        static_cast<uint16_t>(~static_cast<uint16_t>(count));
    output.push_back(static_cast<uint8_t>(inverted));
    output.push_back(static_cast<uint8_t>(inverted >> 8));
    output.insert(
        output.end(), raw.begin() + offset, raw.begin() + offset + count);
    offset += count;
  } while (offset < raw.size());
  append32(output, adler32(raw));
  return output;
}

uint8_t expand5(uint16_t value, ExportFormat format) {
  const uint8_t channel = static_cast<uint8_t>(value & 0x1fu);
  return format == ExportFormat::Rgb565
      ? static_cast<uint8_t>(channel << 3)
      : static_cast<uint8_t>((channel << 3) | (channel >> 2));
}

uint8_t expand6(uint16_t value, ExportFormat format) {
  const uint8_t channel = static_cast<uint8_t>(value & 0x3fu);
  return format == ExportFormat::Rgb565
      ? static_cast<uint8_t>(channel << 2)
      : static_cast<uint8_t>((channel << 2) | (channel >> 4));
}

}  // namespace

bool write(
    const std::filesystem::path& path,
    const Sketch& sketch,
    bool scaled,
    ExportFormat format) {
  if (!isSupportedGridSize(sketch.gridSize) ||
      !isSupportedPaletteSize(sketch.paletteSize)) {
    return false;
  }
  const int size = scaled ? 128 : sketch.gridSize;
  const int scale = scaled ? 128 / sketch.gridSize : 1;
  std::vector<uint8_t> raw;
  raw.reserve(static_cast<std::size_t>(size) * (size * 4u + 1u));
  for (int y = 0; y < size; ++y) {
    raw.push_back(0);
    for (int x = 0; x < size; ++x) {
      const uint8_t index = sketch.pixels[y / scale][x / scale];
      if (index == 0 || index > sketch.paletteSize) {
        raw.insert(raw.end(), {0, 0, 0, 0});
        continue;
      }
      const uint16_t color = sketch.paletteColors[index - 1];
      raw.push_back(expand5(color >> 11, format));
      raw.push_back(expand6(color >> 5, format));
      raw.push_back(expand5(color, format));
      raw.push_back(255);
    }
  }

  std::vector<uint8_t> png = {137, 80, 78, 71, 13, 10, 26, 10};
  std::vector<uint8_t> header;
  append32(header, static_cast<uint32_t>(size));
  append32(header, static_cast<uint32_t>(size));
  header.insert(header.end(), {8, 6, 0, 0, 0});
  appendChunk(png, {'I', 'H', 'D', 'R'}, header);
  appendChunk(png, {'I', 'D', 'A', 'T'}, deflateStored(raw));
  appendChunk(png, {'I', 'E', 'N', 'D'}, {});

  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.write(
      reinterpret_cast<const char*>(png.data()),
      static_cast<std::streamsize>(png.size()));
  return output.good();
}

}  // namespace PngExport
}  // namespace Desktop
}  // namespace bitmap16
