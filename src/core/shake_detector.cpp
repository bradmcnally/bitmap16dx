#include "core/shake_detector.h"

#include "core/clock.h"

namespace bitmap16 {

ShakeDetector::ShakeDetector(float thresholdG, uint32_t cooldownMs)
    : thresholdSquared_(thresholdG * thresholdG),
      cooldownMs_(cooldownMs) {}

bool ShakeDetector::update(
    const AccelerationSample& sample,
    uint32_t nowMs) {
  if (!sample.available ||
      !Clock::hasElapsed(nowMs, lastShakeTime_, cooldownMs_)) {
    return false;
  }

  const float magnitudeSquared =
      sample.x * sample.x +
      sample.y * sample.y +
      sample.z * sample.z;
  if (magnitudeSquared <= thresholdSquared_) {
    return false;
  }

  lastShakeTime_ = nowMs;
  return true;
}

void ShakeDetector::reset() {
  lastShakeTime_ = 0;
}

}  // namespace bitmap16
