#include "core/settings.h"

namespace bitmap16 {

namespace {

uint8_t clamp(uint8_t value, uint8_t minimum, uint8_t maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

}  // namespace

Settings normalizeSettings(Settings settings) {
  if (settings.theme != ThemeId::Light &&
      settings.theme != ThemeId::Dark) {
    settings.theme = ThemeId::Light;
  }
  if (settings.defaultGridSize != 8 &&
      settings.defaultGridSize != 16 &&
      settings.defaultGridSize != 32) {
    settings.defaultGridSize = 8;
  }
  if (settings.matrixUnits != 1 && settings.matrixUnits != 4) {
    settings.matrixUnits = 1;
  }
  settings.matrixRotation %= 4;
  if (settings.exportFormat != ExportFormat::Rgb888 &&
      settings.exportFormat != ExportFormat::Rgb565) {
    settings.exportFormat = ExportFormat::Rgb888;
  }
  settings.displayBrightness =
      clamp(settings.displayBrightness, 10, 100);
  settings.matrixBrightness =
      clamp(settings.matrixBrightness, 1, 20);
  return settings;
}

}  // namespace bitmap16
