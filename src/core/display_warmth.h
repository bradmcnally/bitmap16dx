#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bitmap16 {

// Adjust only the final LCD frame. The caller's canvas and artwork stay intact.
class DisplayWarmth {
public:
  const uint16_t* apply(
      const uint16_t* source, std::size_t count, uint8_t level) {
    if (level == 0 || level > 3) return source;
    if (level != level_) {
      constexpr unsigned greenGain[] = {100, 99, 97, 95};
      constexpr unsigned blueGain[] = {100, 96, 90, 84};
      for (unsigned value = 0; value < green_.size(); ++value) {
        green_[value] = static_cast<uint16_t>(
            ((value * greenGain[level] + 50) / 100) << 5);
      }
      for (unsigned value = 0; value < blue_.size(); ++value) {
        blue_[value] = static_cast<uint16_t>(
            (value * blueGain[level] + 50) / 100);
      }
      level_ = level;
    }
    output_.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
      const uint16_t color = source[index];
      output_[index] = static_cast<uint16_t>(
          (color & 0xf800) | green_[(color >> 5) & 0x3f] |
          blue_[color & 0x1f]);
    }
    return output_.data();
  }

private:
  uint8_t level_ = 0;
  std::array<uint16_t, 64> green_{};
  std::array<uint16_t, 32> blue_{};
  std::vector<uint16_t> output_;
};

}  // namespace bitmap16
