#pragma once

#include <cstdint>
#include <vector>

#include "core/sketch.h"

namespace bitmap16 {
namespace Desktop {
namespace SteamScreenshot {

constexpr int kWidth = 1280;
constexpr int kHeight = 800;

struct Image {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> rgb;
};

bool render(const Sketch& sketch, uint16_t background, Image& image);

}  // namespace SteamScreenshot
}  // namespace Desktop
}  // namespace bitmap16
