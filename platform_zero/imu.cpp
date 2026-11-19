#include "imu.h"

#include <cmath>
#include <fstream>
#include <sstream>

#include "core/clock.h"

namespace bitmap16::Zero {
namespace {

constexpr double kGravity = 9.80665;

template <typename T>
bool readNumber(const std::filesystem::path& path, T& value) {
  std::ifstream input(path);
  if (!(input >> value)) return false;
  input >> std::ws;
  return input.eof();
}

bool usableScale(double scale) {
  // BMI270 has signed 16-bit samples. Require the ±8 g or ±16 g range
  // for the ADV's >6 g gesture, allowing for rounded sysfs scale values.
  const double rangeG = scale * 32768.0 / kGravity;
  return std::isfinite(scale) && rangeG >= 7.9 && rangeG <= 16.1;
}

bool prepareScale(const std::filesystem::path& device) {
  double scale = 0;
  if (!readNumber(device / "in_accel_scale", scale)) return false;
  if (usableScale(scale)) return true;
  if (!std::isfinite(scale) || scale <= 0) return false;

  // Some images start in ±2 g. Ask the kernel for an advertised range;
  // never take over the kernel-owned I2C device or require root to run.
  std::ifstream available(device / "in_accel_scale_available");
  std::string token, selected;
  double selectedScale = 0;
  while (available >> token) {
    std::istringstream number(token);
    double candidate = 0;
    if (!(number >> candidate)) continue;
    number >> std::ws;
    if (number.eof() && usableScale(candidate) &&
        (selected.empty() || candidate < selectedScale)) {
      selected = token;
      selectedScale = candidate;
    }
  }
  if (selected.empty()) return false;
  {
    std::ofstream output(device / "in_accel_scale");
    if (!(output << selected << '\n')) return false;
  }
  return readNumber(device / "in_accel_scale", scale) && usableScale(scale);
}

}  // namespace

bool Imu::open(const std::filesystem::path& root) {
  device_.clear();
  error_ = "BMI270 acceleration unavailable: check the Linux IIO driver and read permissions";
  detector_.reset();
  sampled_ = false;
  std::error_code error;
  std::filesystem::directory_iterator entry(root, error), end;
  for (; !error && entry != end; entry.increment(error)) {
    if (entry->path().filename().string().find("iio:device") != 0) continue;
    std::string name;
    std::ifstream(entry->path() / "name") >> name;
    // M5Stack's image reports "bmi27"; other drivers report "bmi270".
    if (name != "bmi27" && name != "bmi270") continue;
    if (!prepareScale(entry->path())) {
      error_ = "BMI270 needs a readable acceleration scale and an accessible ±8 g or ±16 g range";
      continue;
    }
    device_ = entry->path();
    if (readAcceleration().available) {
      error_.clear();
      return true;
    }
    device_.clear();
  }
  return false;
}

AccelerationSample Imu::readAcceleration() const {
  if (!isAvailable()) return {};
  double scale = 0;
  int raw[3] = {};
  if (!readNumber(device_ / "in_accel_scale", scale) || !usableScale(scale) ||
      !readNumber(device_ / "in_accel_x_raw", raw[0]) ||
      !readNumber(device_ / "in_accel_y_raw", raw[1]) ||
      !readNumber(device_ / "in_accel_z_raw", raw[2])) {
    return {};
  }
  for (int value : raw) {
    if (value < -32768 || value > 32767) return {};
  }
  return {
      static_cast<float>(raw[0] * scale / kGravity),
      static_cast<float>(raw[1] * scale / kGravity),
      static_cast<float>(raw[2] * scale / kGravity),
      true,
  };
}

bool Imu::pollShake(uint32_t nowMs, bool enabled) {
  if (!enabled || !isAvailable()) {
    sampled_ = false;
    return false;
  }
  if (waitTimeout(nowMs, enabled) != 0) return false;
  sampled_ = true;
  lastSampleAt_ = nowMs;
  return detector_.update(readAcceleration(), nowMs);
}

int Imu::waitTimeout(uint32_t nowMs, bool enabled) const {
  if (!enabled || !isAvailable()) return -1;
  const uint32_t elapsed = Clock::elapsed(nowMs, lastSampleAt_);
  return !sampled_ || elapsed >= kSampleIntervalMs
      ? 0 : static_cast<int>(kSampleIntervalMs - elapsed);
}

}  // namespace bitmap16::Zero
