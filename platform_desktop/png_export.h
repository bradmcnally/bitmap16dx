#pragma once

#include <filesystem>

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

}  // namespace PngExport
}  // namespace Desktop
}  // namespace bitmap16
