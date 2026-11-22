#pragma once

#include "keyboard_state.h"
#include "keyboard_discovery.h"

#include <string>
#include <vector>

namespace bitmap16::Zero {

// Linux evdev transport. Rescans periodically to accept keyboards at any time.
class KeyboardDevices {
 public:
  KeyboardDevices() = default;
  ~KeyboardDevices();
  KeyboardDevices(const KeyboardDevices&) = delete;
  KeyboardDevices& operator=(const KeyboardDevices&) = delete;
  bool open(Uint32 backlightEventType);
  int pollEvent(SDL_Event* event);
  void wait(int timeoutMs) const;
  const Uint8* keys() const { return state_.keys(); }
  bool externalActive() const { return state_.externalActive(); }

 private:
  struct Device {
    int fd;
    std::string path;
    bool builtIn;
    bool dropped = false;
  };
  void discover();
  void remove(std::size_t index);
  std::vector<Device> devices_;
  KeyboardState state_;
  KeyboardDiscovery discovery_;
  Uint32 nextDiscovery_ = 0;
  Uint32 backlightEventType_ = static_cast<Uint32>(-1);
  std::size_t nextDevice_ = 0;
};

}  // namespace bitmap16::Zero
