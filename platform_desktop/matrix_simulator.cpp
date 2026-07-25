#include "matrix_simulator.h"

#include <array>
#include <algorithm>

#include "core/led_mapping.h"

namespace bitmap16 {
namespace Desktop {

namespace {

LedMapping::Rgb888 brighten(
    LedMapping::Rgb888 color,
    uint8_t brightness) {
  const int level = std::max(1, std::min(20, static_cast<int>(brightness)));
  color.red = static_cast<uint8_t>(color.red * level / 20);
  color.green = static_cast<uint8_t>(color.green * level / 20);
  color.blue = static_cast<uint8_t>(color.blue * level / 20);
  return color;
}

}  // namespace

MatrixSimulator::~MatrixSimulator() {
  destroy();
}

bool MatrixSimulator::create() {
  window_ = SDL_CreateWindow(
      "BitMap16 DX RGB Matrix",
      SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED,
      360,
      360,
      SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  renderer_ = window_ == nullptr
      ? nullptr
      : SDL_CreateRenderer(
            window_,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (window_ == nullptr || renderer_ == nullptr) {
    destroy();
    return false;
  }
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
  return true;
}

void MatrixSimulator::destroy() {
  if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
  if (window_ != nullptr) SDL_DestroyWindow(window_);
  renderer_ = nullptr;
  window_ = nullptr;
}

void MatrixSimulator::toggle() {
  enabled_ = !enabled_;
}

Uint32 MatrixSimulator::windowId() const {
  return window_ == nullptr ? 0 : SDL_GetWindowID(window_);
}

void MatrixSimulator::render(
    const Sketch& sketch,
    const Settings& settings,
    bool showCursor,
    uint8_t cursorX,
    uint8_t cursorY) {
  if (renderer_ == nullptr) return;
  SDL_SetRenderDrawColor(renderer_, 12, 12, 14, 255);
  SDL_RenderClear(renderer_);
  if (!enabled_ ||
      (sketch.gridSize == 16 && settings.matrixUnits == 1)) {
    SDL_RenderPresent(renderer_);
    return;
  }

  const int matrixSize = settings.matrixUnits == 4 ? 16 : 8;
  std::array<LedMapping::Rgb888, 256> leds = {};
  for (int y = 0; y < sketch.gridSize; ++y) {
    for (int x = 0; x < sketch.gridSize; ++x) {
      LedMapping::Rgb888 color = {};
      const uint8_t pixel = sketch.pixels[y][x];
      if (pixel > 0 && pixel <= sketch.paletteSize) {
        color = LedMapping::rgb565ToRgb888(
            sketch.paletteColors[pixel - 1]);
      }
      if (showCursor && x == cursorX && y == cursorY) {
        color.red = static_cast<uint8_t>(
            std::min(255, static_cast<int>(color.red) + 80));
        color.green = static_cast<uint8_t>(
            std::min(255, static_cast<int>(color.green) + 80));
        color.blue = static_cast<uint8_t>(
            std::min(255, static_cast<int>(color.blue) + 80));
      }
      const int scale = sketch.gridSize == 8 && matrixSize == 16 ? 2 : 1;
      for (int dy = 0; dy < scale; ++dy) {
        for (int dx = 0; dx < scale; ++dx) {
          const uint8_t outputX = static_cast<uint8_t>(x * scale + dx);
          const uint8_t outputY = static_cast<uint8_t>(y * scale + dy);
          const uint16_t index = LedMapping::indexFor(
              outputX,
              outputY,
              settings.matrixUnits,
              settings.matrixRotation);
          leds[index] = brighten(color, settings.matrixBrightness);
        }
      }
    }
  }

  int width = 0;
  int height = 0;
  SDL_GetRendererOutputSize(renderer_, &width, &height);
  const int margin = 12;
  const int cell =
      std::max(1, std::min(width, height) - margin * 2) / matrixSize;
  const int originX = (width - cell * matrixSize) / 2;
  const int originY = (height - cell * matrixSize) / 2;
  for (int y = 0; y < matrixSize; ++y) {
    for (int x = 0; x < matrixSize; ++x) {
      const uint16_t physicalIndex =
          LedMapping::indexFor(
              static_cast<uint8_t>(x),
              static_cast<uint8_t>(y),
              settings.matrixUnits,
              0);
      const LedMapping::Rgb888 color = leds[physicalIndex];
      SDL_SetRenderDrawColor(
          renderer_, color.red, color.green, color.blue, 255);
      SDL_Rect led = {
          originX + x * cell + 2,
          originY + y * cell + 2,
          std::max(1, cell - 4),
          std::max(1, cell - 4),
      };
      SDL_RenderFillRect(renderer_, &led);
      SDL_SetRenderDrawColor(renderer_, 38, 38, 42, 255);
      SDL_Rect border = {
          originX + x * cell,
          originY + y * cell,
          cell,
          cell,
      };
      SDL_RenderDrawRect(renderer_, &border);
    }
  }
  SDL_RenderPresent(renderer_);
}

}  // namespace Desktop
}  // namespace bitmap16
