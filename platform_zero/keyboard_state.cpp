#include "keyboard_state.h"
#include "keyboard_event.h"

#include <algorithm>

namespace bitmap16::Zero {
namespace {
constexpr Uint32 kRepeatDelayMs = 260;
constexpr Uint32 kRepeatIntervalMs = 90;
}  // namespace

void KeyboardState::addDevice(int id, bool builtIn) {
  if (std::none_of(devices_.begin(), devices_.end(),
                   [id](const Device& device) { return device.id == id; })) {
    devices_.push_back({id, builtIn, {}});
    if (!builtIn) externalActive_ = true;
  }
}

SDL_Scancode KeyboardState::control(const Device& device, SDL_Scancode scan) {
  if (scan == SDL_SCANCODE_KP_ENTER) return SDL_SCANCODE_RETURN;
  // The built-in Fn+Backspace still clears; USB Delete erases like Backspace.
  if (!device.builtIn && scan == SDL_SCANCODE_DELETE)
    return SDL_SCANCODE_BACKSPACE;
  return scan;
}

void KeyboardState::rebuildKeys() {
  keys_.fill(0);
  for (const auto& device : devices_) {
    for (int scan = 1; scan < SDL_NUM_SCANCODES; ++scan) {
      if (device.keys[scan]) keys_[control(device, static_cast<SDL_Scancode>(scan))] = 1;
    }
  }
}

void KeyboardState::removeDevice(int id) {
  const auto before = keys_;
  const auto device = std::find_if(devices_.begin(), devices_.end(),
      [id](const Device& entry) { return entry.id == id; });
  if (device == devices_.end()) return;
  const bool builtIn = device->builtIn;
  devices_.erase(std::remove_if(devices_.begin(), devices_.end(),
      [id](const Device& device) { return device.id == id; }), devices_.end());
  if (std::none_of(devices_.begin(), devices_.end(),
      [](const Device& device) { return !device.builtIn; })) externalActive_ = false;
  rebuildKeys();
  for (int scan = 1; scan < SDL_NUM_SCANCODES; ++scan) {
    if (before[scan] && !keys_[scan])
      enqueue(static_cast<SDL_Scancode>(scan), false, false, SDL_GetTicks(), builtIn);
  }
}

void KeyboardState::key(int id, SDL_Scancode scan, bool pressed, bool repeat,
                        Uint32 now, SDL_Keycode symbol) {
  const auto device = std::find_if(devices_.begin(), devices_.end(),
      [id](const Device& entry) { return entry.id == id; });
  if (device == devices_.end() || scan < SDL_SCANCODE_UNKNOWN ||
      scan >= SDL_NUM_SCANCODES) return;
  if (pressed && !repeat) externalActive_ = !device->builtIn;
  // Some built-in Sym characters have no SDL scancode, but are valid text keys.
  if (scan == SDL_SCANCODE_UNKNOWN) {
    if (symbol != SDLK_UNKNOWN) enqueue(scan, pressed, repeat, now, device->builtIn, symbol);
    return;
  }
  const SDL_Scancode mapped = control(*device, scan);
  const bool wasHeld = keys_[mapped] != 0;
  const bool sourceHeld = device->keys[scan] != 0;
  device->keys[scan] = pressed;
  rebuildKeys();
  const auto direction = std::find(kRepeatKeys.begin(), kRepeatKeys.end(), mapped);
  const bool letter = mapped == SDL_SCANCODE_F || mapped == SDL_SCANCODE_Z ||
      mapped == SDL_SCANCODE_X || mapped == SDL_SCANCODE_C;
  if (direction != kRepeatKeys.end() && (!letter || device->builtIn)) {
    // Kernel autorepeat switches to the most recent key (often Draw). Own the
    // navigation repeat so arrows keep moving while another key is held.
    if (repeat) return;
    if (pressed && !sourceHeld)
      nextRepeat_[direction - kRepeatKeys.begin()] = now + kRepeatDelayMs;
  }
  if (repeat || (pressed && !sourceHeld && !wasHeld) ||
      (!pressed && wasHeld && !keys_[mapped]))
    enqueue(mapped, pressed, repeat, now, device->builtIn, symbol);
}

void KeyboardState::enqueue(SDL_Scancode scan, bool pressed, bool repeat,
                            Uint32 now, bool builtIn, SDL_Keycode symbol) {
  const Uint16 modifiers =
      (keys_[SDL_SCANCODE_LSHIFT] ? KMOD_LSHIFT : 0) |
      (keys_[SDL_SCANCODE_RSHIFT] ? KMOD_RSHIFT : 0) |
      (keys_[SDL_SCANCODE_LCTRL] ? KMOD_LCTRL : 0) |
      (keys_[SDL_SCANCODE_RCTRL] ? KMOD_RCTRL : 0) |
      (keys_[SDL_SCANCODE_LALT] ? KMOD_LALT : 0) |
      (keys_[SDL_SCANCODE_RALT] ? KMOD_RALT : 0);
  SDL_Event event{};
  event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
  event.key.timestamp = now;
  event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
  event.key.repeat = repeat;
  event.key.keysym = {scan, SDL_GetKeyFromScancode(scan), modifiers,
                     builtIn ? kBuiltInKeyboardEvent : 0};
  if (symbol != SDLK_UNKNOWN) {
    event.key.keysym.sym = symbol;
    // The Zero's Sym legends already specify their exact printed character.
    event.key.keysym.mod &= ~KMOD_SHIFT;
  }
  events_.push_back(event);
}

bool KeyboardState::pollEvent(SDL_Event* event, Uint32 now, bool allowRepeat) {
  if (!events_.empty()) {
    *event = events_.front();
    events_.pop_front();
    return true;
  }
  if (!allowRepeat) return false;
  for (std::size_t i = 0; i < kRepeatKeys.size(); ++i) {
    const SDL_Scancode scan = kRepeatKeys[i];
    if (keys_[scan] && SDL_TICKS_PASSED(now, nextRepeat_[i])) {
      const bool letter = scan == SDL_SCANCODE_F || scan == SDL_SCANCODE_Z ||
          scan == SDL_SCANCODE_X || scan == SDL_SCANCODE_C;
      const auto owner = std::find_if(devices_.begin(), devices_.end(),
          [scan, letter](const Device& device) {
            return device.keys[scan] && (!letter || device.builtIn);
          });
      if (owner == devices_.end()) continue;
      nextRepeat_[i] = now + kRepeatIntervalMs;
      enqueue(scan, true, true, now, owner->builtIn);
      return pollEvent(event, now);
    }
  }
  return false;
}

}  // namespace bitmap16::Zero
