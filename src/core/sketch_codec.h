#pragma once

#include <cstddef>
#include <cstdint>

#include "core/sketch.h"

namespace bitmap16 {
namespace SketchCodec {

constexpr uint8_t kCurrentVersion = 2;
constexpr std::size_t kLegacyFileSize = 290;
constexpr std::size_t kCurrentFileSize = 291;

enum class Result {
  Ok,
  BufferTooSmall,
  InvalidSize,
  UnsupportedVersion,
  InvalidData,
};

Result encode(
    const Sketch& sketch,
    uint8_t* output,
    std::size_t outputCapacity,
    std::size_t& bytesWritten);
Result decode(const uint8_t* data, std::size_t size, Sketch& sketch);

}  // namespace SketchCodec
}  // namespace bitmap16
