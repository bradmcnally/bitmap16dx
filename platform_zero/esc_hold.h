#pragma once
#include <cstdint>

namespace bitmap16::Zero {

class EscHold {
 public:
  void setPressed(bool pressed, uint32_t now) {
    if (pressed && !pressed_) started_ = now;
    pressed_ = pressed;
  }
  void update(uint32_t now) {
    if (pressed_ && now - started_ >= 3000u) homeRequested_ = true;
  }
  int secondsRemaining(uint32_t now) const {
    if (!pressed_ || homeRequested_) return 0;
    const uint32_t elapsed = now - started_;
    if (elapsed < 500u || elapsed >= 3000u) return 0;
    return static_cast<int>((3000u - elapsed + 999u) / 1000u);
  }
  bool pressed() const { return pressed_; }
  bool homeRequested() const { return homeRequested_; }

 private:
  bool pressed_ = false;
  bool homeRequested_ = false;
  uint32_t started_ = 0;
};

}  // namespace bitmap16::Zero
