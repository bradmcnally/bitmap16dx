#pragma once

#include <SDL.h>

#include <cstddef>
#include <cstdint>

#include <linux/fb.h>

#include "core/display_warmth.h"
#include "core/canvas.h"
#include "esc_hold.h"
#include "keyboard_devices.h"
#include <vector>

namespace bitmap16::Zero {

class NativeIo {
public:
  NativeIo() = default;
  ~NativeIo();
  NativeIo(const NativeIo&) = delete;
  NativeIo& operator=(const NativeIo&) = delete;

  bool open();
  void present(const uint16_t* pixels, int width, int height);
  void setDisplayWarmth(uint8_t level) { displayWarmth_ = level; }
  int pollEvent(SDL_Event* event);
  int waitEvent(SDL_Event* event, int timeoutMs);
  const Uint8* keyboardState() const { return keyboards_.keys(); }
  bool externalKeyboardActive() const { return keyboards_.externalActive(); }
  bool isHomeEvent(const SDL_Event& event) const { return event.type == homeEventType_; }
  bool escapeHeld() const { return escHold_.pressed(); }
  bool isBacklightEvent(const SDL_Event& event) const {
    return event.type == backlightEventType_;
  }

private:
  void writeFrame(const uint16_t* pixels, int width, int height);
  void redrawEscCountdown();
  bool updateEsc(SDL_Event* event);

  Uint32 backlightEventType_ = static_cast<Uint32>(-1);
  Uint32 homeEventType_ = static_cast<Uint32>(-1);
  Uint32 keyboardLayoutEventType_ = static_cast<Uint32>(-1);
  EscHold escHold_;
  int countdown_ = 0;
  std::vector<uint16_t> lastFrame_;
  Canvas countdownFrame_;
  int frameWidth_ = 0;
  int frameHeight_ = 0;
  int framebuffer_ = -1;
  KeyboardDevices keyboards_;
  uint8_t* mapping_ = nullptr;
  std::size_t mappingSize_ = 0;
  bool directWrite_ = false;
  uint8_t displayWarmth_ = 0;
  DisplayWarmth warmthFilter_;
  fb_fix_screeninfo fixed_{};
  fb_var_screeninfo mode_{};
};

}  // namespace bitmap16::Zero
