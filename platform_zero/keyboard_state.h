#pragma once

#include <SDL.h>

#include <array>
#include <deque>
#include <vector>

namespace bitmap16::Zero {

// Keeps physical keyboards independent while exposing one set of held controls.
class KeyboardState {
 public:
  void addDevice(int id, bool builtIn);
  void removeDevice(int id);
  void key(int id, SDL_Scancode scan, bool pressed, bool repeat, Uint32 now,
           SDL_Keycode symbol = SDLK_UNKNOWN);
  bool pollEvent(SDL_Event* event, Uint32 now, bool allowRepeat = true);
  const Uint8* keys() const { return keys_.data(); }
  bool externalActive() const { return externalActive_; }

 private:
  struct Device {
    int id;
    bool builtIn;
    std::array<Uint8, SDL_NUM_SCANCODES> keys{};
  };
  static SDL_Scancode control(const Device& device, SDL_Scancode scan);
  void rebuildKeys();
  void enqueue(SDL_Scancode scan, bool pressed, bool repeat,
               Uint32 now, bool builtIn, SDL_Keycode symbol = SDLK_UNKNOWN);

  std::vector<Device> devices_;
  std::array<Uint8, SDL_NUM_SCANCODES> keys_{};
  std::deque<SDL_Event> events_;
  bool externalActive_ = false;
  static constexpr std::array kRepeatKeys = {
      SDL_SCANCODE_UP, SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_RIGHT,
      SDL_SCANCODE_F, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C};
  std::array<Uint32, kRepeatKeys.size()> nextRepeat_{};
};

}  // namespace bitmap16::Zero
