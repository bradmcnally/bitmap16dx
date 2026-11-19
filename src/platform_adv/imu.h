#pragma once

#include "core/shake_detector.h"

namespace IMU {

void init();
bool isAvailable();
bitmap16::AccelerationSample readAcceleration();

}  // namespace IMU
