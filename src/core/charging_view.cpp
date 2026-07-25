#include "core/charging_view.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bitmap16 {
namespace ChargingView {

namespace {

constexpr int kIconSize = 24;
constexpr int kSketchSize = 48;

uint32_t nextRandom(uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return state;
}

int itemWidth(int index) {
  if (index == kSketchItem) return kSketchSize;
  if (index == kBatteryItem) return 54;
  return kIconSize;
}

int itemHeight(int index) {
  return index == kSketchItem ? kSketchSize : kIconSize;
}

void drawIcon(
    Canvas& canvas,
    const Item& item,
    const Theme& theme) {
  if (item.icon == nullptr) {
    return;
  }
  const int originX = static_cast<int>(item.x);
  const int originY = static_cast<int>(item.y);
  for (int row = 0; row < kIconSize; ++row) {
    for (int column = 0; column < kIconSize; ++column) {
      const int pixelIndex = row * kIconSize + column;
      const int bitShift = (3 - (pixelIndex % 4)) * 2;
      const uint8_t value =
          (item.icon[pixelIndex / 4] >> bitShift) & 0x03;
      if (value == 1) {
        canvas.drawPixel(originX + column, originY + row, theme.iconDark);
      } else if (value == 2) {
        canvas.drawPixel(originX + column, originY + row, theme.iconLight);
      }
    }
  }
}

void drawSketch(
    Canvas& canvas,
    const Item& item,
    const SketchImage& sketch) {
  if (sketch.pixels == nullptr || sketch.paletteColors == nullptr ||
      (sketch.gridSize != 8 && sketch.gridSize != 16)) {
    return;
  }
  const int cellSize = kSketchSize / sketch.gridSize;
  const int originX = static_cast<int>(item.x);
  const int originY = static_cast<int>(item.y);
  for (int y = 0; y < sketch.gridSize; ++y) {
    for (int x = 0; x < sketch.gridSize; ++x) {
      const uint8_t index = sketch.pixels[y][x];
      if (index == 0 || index > sketch.paletteSize) {
        continue;
      }
      canvas.fillRect(
          originX + x * cellSize,
          originY + y * cellSize,
          cellSize,
          cellSize,
          sketch.paletteColors[index - 1]);
    }
  }
}

}  // namespace

void initialize(
    State& state,
    int width,
    int height,
    uint32_t seed,
    const uint8_t* const icons[4],
    int batteryPercent,
    bool sketchAvailable) {
  state = {};
  state.batteryPercent = std::max(0, std::min(100, batteryPercent));
  state.sketchAvailable = sketchAvailable;
  if (seed == 0) {
    seed = 1;
  }

  for (int i = 0; i < kItemCount; ++i) {
    const int maximumX = std::max(0, width - itemWidth(i));
    const int maximumY = std::max(0, height - itemHeight(i));
    bool tooClose = false;
    int attempts = 0;
    do {
      state.items[i].x = static_cast<float>(
          maximumX == 0 ? 0 : nextRandom(seed) % (maximumX + 1));
      state.items[i].y = static_cast<float>(
          maximumY == 0 ? 0 : nextRandom(seed) % (maximumY + 1));
      tooClose = false;
      for (int previous = 0; previous < i; ++previous) {
        if (std::fabs(state.items[i].x - state.items[previous].x) < 48.0f &&
            std::fabs(state.items[i].y - state.items[previous].y) < 48.0f) {
          tooClose = true;
          break;
        }
      }
    } while (tooClose && ++attempts < 50);

    const float speedX =
        0.7f + static_cast<float>(nextRandom(seed) % 61) / 100.0f;
    const float speedY =
        0.7f + static_cast<float>(nextRandom(seed) % 61) / 100.0f;
    state.items[i].dx = nextRandom(seed) & 1u ? speedX : -speedX;
    state.items[i].dy = nextRandom(seed) & 1u ? speedY : -speedY;
    state.items[i].icon = i < 4 ? icons[i] : nullptr;
  }
}

void setBattery(State& state, int batteryPercent, const uint8_t* icon) {
  state.batteryPercent = std::max(0, std::min(100, batteryPercent));
  state.items[kBatteryItem].icon = icon;
}

void update(State& state, int width, int height) {
  const int activeCount = state.sketchAvailable
      ? kItemCount
      : kItemCount - 1;
  for (int i = 0; i < activeCount; ++i) {
    Item& item = state.items[i];
    item.x += item.dx;
    item.y += item.dy;
    const float maximumX =
        static_cast<float>(std::max(0, width - itemWidth(i)));
    const float maximumY =
        static_cast<float>(std::max(0, height - itemHeight(i)));
    if (item.x <= 0.0f) {
      item.x = 0.0f;
      item.dx = std::fabs(item.dx);
    } else if (item.x >= maximumX) {
      item.x = maximumX;
      item.dx = -std::fabs(item.dx);
    }
    if (item.y <= 0.0f) {
      item.y = 0.0f;
      item.dy = std::fabs(item.dy);
    } else if (item.y >= maximumY) {
      item.y = maximumY;
      item.dy = -std::fabs(item.dy);
    }
  }

  for (int first = 0; first < activeCount; ++first) {
    for (int second = first + 1; second < activeCount; ++second) {
      const int threshold =
          first == kSketchItem || second == kSketchItem ? 30 : 16;
      Item& a = state.items[first];
      Item& b = state.items[second];
      if (std::fabs(a.x - b.x) >= threshold ||
          std::fabs(a.y - b.y) >= threshold) {
        continue;
      }
      std::swap(a.dx, b.dx);
      std::swap(a.dy, b.dy);
      a.x += a.dx * 2.0f;
      a.y += a.dy * 2.0f;
      b.x += b.dx * 2.0f;
      b.y += b.dy * 2.0f;
    }
  }
}

void render(
    Canvas& canvas,
    const State& state,
    const Theme& theme,
    const SketchImage* sketch) {
  if (!canvas.isValid()) {
    return;
  }
  canvas.fillScreen(theme.background);
  const int activeCount = state.sketchAvailable && sketch != nullptr
      ? kItemCount
      : kItemCount - 1;
  for (int i = 0; i < activeCount; ++i) {
    if (i == kSketchItem) {
      drawSketch(canvas, state.items[i], *sketch);
    } else {
      drawIcon(canvas, state.items[i], theme);
    }
  }

  char batteryText[8] = {};
  std::snprintf(batteryText, sizeof(batteryText), "%d%%", state.batteryPercent);
  canvas.setTextAlign(TextAlign::Left);
  canvas.setTextSize(1);
  canvas.setTextColor(theme.text);
  canvas.drawString(
      batteryText,
      static_cast<int>(state.items[kBatteryItem].x) + 28,
      static_cast<int>(state.items[kBatteryItem].y) + 8);
}

}  // namespace ChargingView
}  // namespace bitmap16
