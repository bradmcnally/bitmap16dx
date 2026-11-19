#pragma once

#include <cstdint>

#include "core/input.h"

namespace Input {

void init();
bitmap16::InputFrame poll(uint32_t nowMs);
void reset();

}  // namespace Input
