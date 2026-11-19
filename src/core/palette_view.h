#pragma once

#include <cstdint>

#include "core/canvas.h"

namespace bitmap16 {
namespace PaletteView {

constexpr float kInsertionDurationMs = 520.0f;
#if defined(BITMAP16_LOSPEC) || defined(BITMAP16_CARDPUTER_ZERO) || defined(BITMAP16_STEAM_DECK)
constexpr int kMaximumEntries = 512;
using CatalogIndex = uint16_t;
#else
constexpr int kMaximumEntries = 32;
using CatalogIndex = uint8_t;
#endif

struct Entry {
  const uint16_t* colors = nullptr;
  const char* name = nullptr;
  uint8_t size = 16;
  bool user = false;
  const char* author = nullptr;
  int likes = -1;
};

struct Catalog {
  const Entry* entries = nullptr;
  int count = 0;
  const char* title = "PALETTES";
  const char* emptyMessage = "NO PALETTES";
};

struct State {
  int cursor = 0;
  float scrollPosition = 0.0f;
  uint8_t filterSize = 0;
  bool userOnly = false;
  CatalogIndex filteredIndices[kMaximumEntries] = {};
  int filteredCount = 0;
  bool reloadAnimating = false;
  float reloadElapsedMs = 0.0f;
  float reloadDurationMs = 0.0f;
  const uint16_t* reloadFrame = nullptr;
  bool exitAnimating = false;
  float exitElapsedMs = 0.0f;
  float exitDurationMs = 0.0f;
  int exitCenter = 0;
  bool entranceAnimating = false;
  float entranceElapsedMs = 0.0f;
  float entranceDurationMs = 0.0f;
  int entranceCenter = 0;
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
  ExitComplete,
};

void reset(State& state, const Catalog& catalog);
void rebuildFilter(State& state, const Catalog& catalog);
bool moveCursor(State& state, int delta);
bool toggleSizeFilter(State& state, const Catalog& catalog, uint8_t size);
uint8_t steppedSizeFilter(uint8_t current, int direction);
bool toggleUserFilter(State& state, const Catalog& catalog);
int selectedCatalogIndex(const State& state);
void selectCatalogIndex(State& state, int catalogIndex);
void beginEntrance(State& state, int viewportWidth);
void beginExit(State& state, int viewportWidth);
// previousFrame must remain valid at the render canvas size until reload completes.
void beginReload(State& state, int viewportWidth, const uint16_t* previousFrame);
bool beginSelection(State& state);
AnimationResult advance(
    State& state,
    float scrollSpeed = 0.25f,
    float insertionSpeed = 0.028f);
AnimationResult advanceTimed(State& state, float elapsedMs);
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
