#pragma once

#include <cstdint>

#include "core/canvas.h"

namespace bitmap16 {
namespace PaletteView {

constexpr float kInsertionDurationMs = 520.0f;
constexpr int kMaximumEntries = 32;

struct Entry {
  const uint16_t* colors = nullptr;
  const char* name = nullptr;
  uint8_t size = 16;
  bool user = false;
};

struct Catalog {
  const Entry* entries = nullptr;
  int count = 0;
};

struct State {
  int cursor = 0;
  float scrollPosition = 0.0f;
  uint8_t filterSize = 0;
  bool userOnly = false;
  uint8_t filteredIndices[kMaximumEntries] = {};
  int filteredCount = 0;
  bool insertionAnimating = false;
  float insertionProgress = 0.0f;
  float frozenScrollPosition = 0.0f;
};

struct Theme {
  uint16_t background;
  uint16_t text;
  uint16_t textSecondary;
  bool dark;
};

enum class AnimationResult : uint8_t {
  Idle,
  Animating,
  SelectionComplete,
};

void reset(State& state, const Catalog& catalog);
void rebuildFilter(State& state, const Catalog& catalog);
bool moveCursor(State& state, int delta);
bool toggleSizeFilter(State& state, const Catalog& catalog, uint8_t size);
bool toggleUserFilter(State& state, const Catalog& catalog);
int selectedCatalogIndex(const State& state);
void selectCatalogIndex(State& state, int catalogIndex);
bool beginSelection(State& state);
AnimationResult advance(
    State& state,
    float scrollSpeed = 0.25f,
    float insertionSpeed = 0.028f);
void render(
    Canvas& canvas,
    const State& state,
    const Catalog& catalog,
    int activeCatalogIndex,
    const Theme& theme,
    const uint16_t* cartridgeGraphic = nullptr,
    const char* statusMessage = nullptr);

}  // namespace PaletteView
}  // namespace bitmap16
