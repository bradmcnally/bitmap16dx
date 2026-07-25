#pragma once

#include <cstdint>

#include "core/canvas.h"

namespace Display {

bool init();
bool isReady();
bitmap16::Canvas& canvas();
void beginFrame(uint16_t clearColor);
bool endFrame();
void setBrightness(uint8_t percent);
void shutdown();

}  // namespace Display
