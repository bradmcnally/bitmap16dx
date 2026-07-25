#pragma once

#include <cstdint>

namespace bitmap16 {

enum class ThemeId : uint8_t {
  Light = 0,
  Dark = 1,
};

enum class ExportFormat : uint8_t {
  Rgb888 = 0,
  Rgb565 = 1,
};

struct Settings {
  ThemeId theme = ThemeId::Light;
  uint8_t defaultGridSize = 8;
  uint8_t matrixUnits = 1;
  uint8_t matrixRotation = 2;
  ExportFormat exportFormat = ExportFormat::Rgb888;
  bool shakeUndoEnabled = false;
  bool matrixEnabled = false;
  uint8_t displayBrightness = 80;
  uint8_t matrixBrightness = 5;
};

Settings normalizeSettings(Settings settings);

}  // namespace bitmap16
