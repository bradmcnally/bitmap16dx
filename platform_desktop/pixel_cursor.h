#pragma once

#include <SDL.h>
#include <algorithm>
#include "core/canvas_view.h"

namespace bitmap16::Desktop {
// Use the canvas artwork as a native mouse cursor so it remains responsive in
// every screen and dialog without needing to repaint the application on motion.
class PixelCursor {
 public:
  ~PixelCursor() { if (SDL_WasInit(SDL_INIT_VIDEO)) clear(); }
  void clear() {
    if (cursor_) {
      SDL_SetCursor(SDL_GetDefaultCursor());
      SDL_FreeCursor(cursor_);
      cursor_ = nullptr;
    }
  }
  void update(const CanvasView::Assets& assets, const CanvasView::Theme& theme,
              int scale) {
    const auto& icon = assets.cursor;
    if (pixels_ == icon.pixels && scale_ == scale &&
        dark_ == theme.iconDark && light_ == theme.iconLight) return;
    pixels_ = icon.pixels; scale_ = scale;
    dark_ = theme.iconDark; light_ = theme.iconLight;
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, icon.width * scale, icon.height * scale, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return;
    SDL_FillRect(surface, nullptr, SDL_MapRGBA(surface->format, 0, 0, 0, 0));
    const auto color = [&](uint16_t value) {
      return SDL_MapRGBA(surface->format, ((value >> 11) & 31) * 255 / 31,
          ((value >> 5) & 63) * 255 / 63, (value & 31) * 255 / 31, 255);
    };
    for (int y = 0; y < icon.height; ++y) {
      for (int x = 0; x < icon.width; ++x) {
        const int index = y * icon.width + x;
        const int value = (icon.pixels[index / 4] >> ((3 - index % 4) * 2)) & 3;
        if (value != 1 && value != 2) continue;
        SDL_Rect block{x * scale, y * scale, scale, scale};
        SDL_FillRect(surface, &block, color(value == 1 ? dark_ : light_));
      }
    }
    SDL_Cursor* next = SDL_CreateColorCursor(surface,
        std::clamp(-assets.cursorOffsetX, 0, icon.width - 1) * scale,
        std::clamp(-assets.cursorOffsetY, 0, icon.height - 1) * scale);
    SDL_FreeSurface(surface);
    if (!next) return;
    SDL_SetCursor(next);
    if (cursor_) SDL_FreeCursor(cursor_);
    cursor_ = next;
  }
 private:
  SDL_Cursor* cursor_ = nullptr;
  const uint8_t* pixels_ = nullptr;
  int scale_ = 0;
  uint16_t dark_ = 0, light_ = 0;
};
}  // namespace bitmap16::Desktop
