#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "core/settings.h"
#include "core/sketch.h"

namespace bitmap16 {
namespace Desktop {
namespace PngExport {

bool write(
    const std::filesystem::path& path,
    const Sketch& sketch,
    bool scaled,
    ExportFormat format);
bool writeRgb(
    const std::filesystem::path& path,
    const std::vector<uint8_t>& rgb,
    int width,
    int height);

}  // namespace PngExport
}  // namespace Desktop
}  // namespace bitmap16
