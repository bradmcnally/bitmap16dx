#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/shake_detector.h"

namespace bitmap16::Zero {

// Linux IIO supplies acceleration in m/s²; the shared detector consumes g.
class Imu {
 public:
  bool open(const std::filesystem::path& root = "/sys/bus/iio/devices");
  bool isAvailable() const { return !device_.empty(); }
  const std::string& error() const { return error_; }
  AccelerationSample readAcceleration() const;

  // Call on every loop iteration, including iterations with keyboard events.
  bool pollShake(uint32_t nowMs, bool enabled);
  // Maximum time the event loop may sleep; -1 means no sensor deadline.
  int waitTimeout(uint32_t nowMs, bool enabled) const;

 private:
  static constexpr uint32_t kSampleIntervalMs = 10;
  std::filesystem::path device_;
  std::string error_;
  ShakeDetector detector_;
  uint32_t lastSampleAt_ = 0;
  bool sampled_ = false;
};

}  // namespace bitmap16::Zero
