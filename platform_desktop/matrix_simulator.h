#pragma once

#include <SDL.h>

#include "core/settings.h"
#include "core/sketch.h"

namespace bitmap16 {
namespace Desktop {

class MatrixSimulator {
 public:
  ~MatrixSimulator();

  bool create();
  void destroy();
  void toggle();
  void setEnabled(bool enabled) { enabled_ = enabled; }
  bool isEnabled() const { return enabled_; }
  Uint32 windowId() const;
  void render(
      const Sketch& sketch,
      const Settings& settings,
      bool showCursor,
      uint8_t cursorX,
      uint8_t cursorY);

 private:
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  bool enabled_ = false;
};

}  // namespace Desktop
}  // namespace bitmap16
