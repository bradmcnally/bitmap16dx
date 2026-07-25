#pragma once

#include <cstdint>

namespace LEDMatrix {

bool init();
void setConfiguration(uint8_t matrixUnits, uint8_t rotation);
void setEnabled(bool enabled);
bool isEnabled();
void setBrightness(uint8_t percent);
void clear();
bool setPixelRgb565(uint8_t x, uint8_t y, uint16_t color);
void show();

}  // namespace LEDMatrix
