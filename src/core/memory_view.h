#pragma once

#include <cstdint>

#include "core/canvas.h"
#include "core/sketch.h"

namespace bitmap16 {
namespace MemoryView {

struct Entry {
  const uint8_t* pixels = nullptr;
  uint8_t pixelStride = 0;
  uint8_t gridSize = 8;
  const uint16_t* paletteColors = nullptr;
  uint8_t paletteSize = 16;
  bool active = false;
};

struct Catalog {
  const Entry* entries = nullptr;
  int count = 0;
};

struct State {
  int cursor = 0;
  int scrollOffset = 0;
  float scrollPosition = 0.0f;
  float cursorAnimationPhase = 0.0f;
};

struct VisibleRange {
  int first = 0;
  int last = -1;
};

struct Theme {
  uint16_t background;
  uint16_t thumbnail;
  uint16_t text;
  uint16_t selectionDark;
  uint16_t selectionLight;
  uint16_t active;
};

struct Assets {
  const uint8_t* selectorCorner = nullptr;
  uint8_t selectorWidth = 0;
  uint8_t selectorHeight = 0;
};

int columnCount(int width);
int thumbnailSize();
VisibleRange visibleCatalogRange(
    const State& state,
    int sketchCount,
    int width,
    int height);
void clamp(State& state, int sketchCount);
bool moveCursor(
    State& state,
    int deltaX,
    int deltaY,
    int sketchCount,
    int width);
bool advance(
    State& state,
    int sketchCount,
    int width,
    int height,
    float deltaSeconds,
    float scrollSpeed = 0.35f);
void render(
    Canvas& canvas,
    const State& state,
    const Catalog& catalog,
    const Theme& theme,
    const char* statusMessage = nullptr,
    const Assets* assets = nullptr);

}  // namespace MemoryView
}  // namespace bitmap16
