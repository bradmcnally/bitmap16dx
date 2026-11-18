#pragma once

#include <SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include <linux/fb.h>

namespace bitmap16::Zero {

class NativeIo {
public:
  NativeIo() = default;
  ~NativeIo();
  NativeIo(const NativeIo&) = delete;
  NativeIo& operator=(const NativeIo&) = delete;

  bool open();
  void present(const uint16_t* pixels, int width, int height);
  int pollEvent(SDL_Event* event);
  int waitEvent(SDL_Event* event, int timeoutMs);
  const Uint8* keyboardState() const { return keys_.data(); }
  bool isBacklightEvent(const SDL_Event& event) const {
    return event.type == backlightEventType_;
  }

private:
  void fillKeyEvent(
      SDL_Event* event, SDL_Scancode scan, bool pressed, bool repeat) const;

  Uint32 backlightEventType_ = static_cast<Uint32>(-1);
  int framebuffer_ = -1;
  int keyboard_ = -1;
  uint8_t* mapping_ = nullptr;
  std::size_t mappingSize_ = 0;
  bool directWrite_ = false;
  fb_fix_screeninfo fixed_{};
  fb_var_screeninfo mode_{};
  std::array<Uint8, SDL_NUM_SCANCODES> keys_{};

  static constexpr std::array kDirectionKeys = {
      SDL_SCANCODE_UP, SDL_SCANCODE_LEFT, SDL_SCANCODE_DOWN, SDL_SCANCODE_RIGHT,
      SDL_SCANCODE_F, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C};
  std::array<Uint32, kDirectionKeys.size()> nextDirectionRepeat_{};
};

}  // namespace bitmap16::Zero
