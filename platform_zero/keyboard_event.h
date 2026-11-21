#pragma once

#include <SDL.h>

namespace bitmap16::Zero {

// Native evdev events use SDL's reserved field to retain their keyboard source.
// Ordinary SDL keyboard events (including the simulator) use the USB layout.
constexpr Uint32 kBuiltInKeyboardEvent = 0x425a494b;

inline bool isBuiltInKeyboardEvent(const SDL_Event& event) {
  return (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) &&
         event.key.keysym.unused == kBuiltInKeyboardEvent;
}

// Apply this only in navigation contexts: text fields always keep their letters.
inline SDL_Keycode navigationKey(const SDL_Event& event, bool allowShift = true) {
  const SDL_Keycode key = event.key.keysym.sym;
  const Uint16 modifiers = event.key.keysym.mod;
  if (!isBuiltInKeyboardEvent(event) || (modifiers & (KMOD_CTRL | KMOD_ALT)) ||
      (!allowShift && (modifiers & KMOD_SHIFT))) return key;
  switch (key) {
    case SDLK_f: return SDLK_UP;
    case SDLK_z: return SDLK_LEFT;
    case SDLK_x: return SDLK_DOWN;
    case SDLK_c: return SDLK_RIGHT;
    default: return key;
  }
}

}  // namespace bitmap16::Zero
