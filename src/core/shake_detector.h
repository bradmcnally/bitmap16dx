#pragma once

#include <cstdint>

namespace bitmap16 {

struct AccelerationSample {
  float x = 0.0f;
  float y = 0.0f;
  float z = 0.0f;
  bool available = false;
};

class ShakeDetector {
 public:
  ShakeDetector(float thresholdG = 6.0f, uint32_t cooldownMs = 500);

  bool update(const AccelerationSample& sample, uint32_t nowMs);
  void reset();

 private:
  float thresholdSquared_;
  uint32_t cooldownMs_;
  uint32_t lastShakeTime_ = 0;
};

}  // namespace bitmap16
