#include "keyboard_devices.h"
#include "symbol_keys.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace bitmap16::Zero {
namespace {
constexpr Uint32 kDiscoveryIntervalMs = 500;

int adjustBacklight(bool increase) {
  constexpr const char* kBrightness = "/sys/class/backlight/backlight/brightness";
  constexpr const char* kMaximum = "/sys/class/backlight/backlight/max_brightness";
  int current = 0;
  int maximum = 0;
  if (!(std::ifstream(kBrightness) >> current) ||
      !(std::ifstream(kMaximum) >> maximum) ||
      maximum < 1 || current < 0 || current > maximum) {
    SDL_Log("Zero backlight level unavailable: %s", kBrightness);
    return -1;
  }

  const int step = std::max(1, maximum / 20);
  const int next = increase
      ? current + std::min(step, maximum - current)
      : current <= 1 ? current : std::max(1, current - step);
  if (next != current) {
    std::ofstream brightness(kBrightness);
    brightness << next << '\n';
    brightness.close();
    if (!brightness) {
      SDL_Log("Zero backlight could not be changed: %s", kBrightness);
      return -1;
    }
  }
  return static_cast<int>((100LL * next + maximum / 2) / maximum);
}

SDL_Scancode scanCode(unsigned code) {
  constexpr SDL_Scancode top[] = {
      SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
      SDL_SCANCODE_T, SDL_SCANCODE_Y, SDL_SCANCODE_U, SDL_SCANCODE_I,
      SDL_SCANCODE_O, SDL_SCANCODE_P};
  constexpr SDL_Scancode middle[] = {
      SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
      SDL_SCANCODE_G, SDL_SCANCODE_H, SDL_SCANCODE_J, SDL_SCANCODE_K,
      SDL_SCANCODE_L};
  constexpr SDL_Scancode bottom[] = {
      SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V,
      SDL_SCANCODE_B, SDL_SCANCODE_N, SDL_SCANCODE_M};
  if (code >= KEY_Q && code <= KEY_P) return top[code - KEY_Q];
  if (code >= KEY_A && code <= KEY_L) return middle[code - KEY_A];
  if (code >= KEY_Z && code <= KEY_M) return bottom[code - KEY_Z];
  if (code >= KEY_1 && code <= KEY_0) {
    return static_cast<SDL_Scancode>(SDL_SCANCODE_1 + code - KEY_1);
  }
  if (code >= KEY_F1 && code <= KEY_F10) {
    return static_cast<SDL_Scancode>(SDL_SCANCODE_F1 + code - KEY_F1);
  }
  switch (code) {
    case KEY_ESC: return SDL_SCANCODE_ESCAPE;
    case KEY_ENTER: return SDL_SCANCODE_RETURN;
    case KEY_KPENTER: return SDL_SCANCODE_KP_ENTER;
    case KEY_BACKSPACE: return SDL_SCANCODE_BACKSPACE;
    case KEY_DELETE: return SDL_SCANCODE_DELETE;
    case KEY_TAB: return SDL_SCANCODE_TAB;
    case KEY_SPACE: return SDL_SCANCODE_SPACE;
    case KEY_MINUS: return SDL_SCANCODE_MINUS;
    case KEY_EQUAL: return SDL_SCANCODE_EQUALS;
    case KEY_KPPLUS: return SDL_SCANCODE_KP_PLUS;
    case KEY_KPMINUS: return SDL_SCANCODE_KP_MINUS;
    case KEY_LEFT: return SDL_SCANCODE_LEFT;
    case KEY_RIGHT: return SDL_SCANCODE_RIGHT;
    case KEY_UP: return SDL_SCANCODE_UP;
    case KEY_DOWN: return SDL_SCANCODE_DOWN;
    case KEY_LEFTSHIFT: return SDL_SCANCODE_LSHIFT;
    case KEY_RIGHTSHIFT: return SDL_SCANCODE_RSHIFT;
    case KEY_LEFTCTRL: return SDL_SCANCODE_LCTRL;
    case KEY_RIGHTCTRL: return SDL_SCANCODE_RCTRL;
    case KEY_LEFTALT: return SDL_SCANCODE_LALT;
    case KEY_RIGHTALT: return SDL_SCANCODE_RALT;
    case KEY_GRAVE: return SDL_SCANCODE_GRAVE;
    case KEY_BACKSLASH: return SDL_SCANCODE_BACKSLASH;
    case KEY_102ND: return SDL_SCANCODE_NONUSBACKSLASH;
    case KEY_CAPSLOCK: return SDL_SCANCODE_CAPSLOCK;
    case KEY_KP0: return SDL_SCANCODE_KP_0;
    case KEY_KP1: return SDL_SCANCODE_KP_1;
    case KEY_KP2: return SDL_SCANCODE_KP_2;
    case KEY_KP3: return SDL_SCANCODE_KP_3;
    case KEY_KP4: return SDL_SCANCODE_KP_4;
    case KEY_KP5: return SDL_SCANCODE_KP_5;
    case KEY_KP6: return SDL_SCANCODE_KP_6;
    case KEY_KP7: return SDL_SCANCODE_KP_7;
    case KEY_KP8: return SDL_SCANCODE_KP_8;
    case KEY_KP9: return SDL_SCANCODE_KP_9;
    case KEY_KPDOT: return SDL_SCANCODE_KP_PERIOD;
    case KEY_KPASTERISK: return SDL_SCANCODE_KP_MULTIPLY;
    case KEY_KPSLASH: return SDL_SCANCODE_KP_DIVIDE;
    case KEY_HELP: return SDL_SCANCODE_HELP;
    case KEY_HOME: return SDL_SCANCODE_HOME;
    case KEY_END: return SDL_SCANCODE_END;
    case KEY_PAGEUP: return SDL_SCANCODE_PAGEUP;
    case KEY_PAGEDOWN: return SDL_SCANCODE_PAGEDOWN;
    case KEY_LEFTBRACE: return SDL_SCANCODE_LEFTBRACKET;
    case KEY_RIGHTBRACE: return SDL_SCANCODE_RIGHTBRACKET;
    case KEY_SEMICOLON: return SDL_SCANCODE_SEMICOLON;
    case KEY_APOSTROPHE: return SDL_SCANCODE_APOSTROPHE;
    case KEY_COMMA: return SDL_SCANCODE_COMMA;
    case KEY_DOT: return SDL_SCANCODE_PERIOD;
    case KEY_SLASH: return SDL_SCANCODE_SLASH;
    default: return SDL_SCANCODE_UNKNOWN;
  }
}

bool hasKey(const std::array<unsigned char, (KEY_MAX + 8) / 8>& bits, unsigned key) {
  return (bits[key / 8] & (1u << (key % 8))) != 0;
}

bool isKeyboard(int fd) {
  std::array<unsigned char, (KEY_MAX + 8) / 8> bits{};
  return ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(bits)), bits.data()) >= 0 &&
         hasKey(bits, KEY_A) && hasKey(bits, KEY_Z) &&
         hasKey(bits, KEY_ENTER) && hasKey(bits, KEY_SPACE);
}

bool isBuiltIn(int fd) {
  char name[128] = {};
  if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 &&
      (std::strstr(name, "tca8418") || std::strstr(name, "TCA8418"))) return true;
  // Some images use a generic input name for the built-in I2C keypad.
  struct stat device{}, keypad{};
  return fstat(fd, &device) == 0 &&
         stat("/dev/input/by-path/platform-3f804000.i2c-event", &keypad) == 0 &&
         device.st_rdev == keypad.st_rdev;
}
}  // namespace

KeyboardDevices::~KeyboardDevices() {
  for (const auto& device : devices_) close(device.fd);
}

bool KeyboardDevices::open(Uint32 backlightEventType) {
  backlightEventType_ = backlightEventType;
  discover();
  return !devices_.empty();
}

void KeyboardDevices::discover() {
  nextDiscovery_ = SDL_GetTicks() + kDiscoveryIntervalMs;
  discovery_.beginScan();
  std::error_code error;
  std::filesystem::directory_iterator entry("/dev/input", error);
  for (const std::filesystem::directory_iterator end; !error && entry != end;
       entry.increment(error)) {
    const auto path = entry->path();
    if (path.filename().string().compare(0, 5, "event") != 0) continue;
    if (std::any_of(devices_.begin(), devices_.end(),
        [&](const Device& device) { return device.path == path.string(); })) continue;
    struct stat node{};
    if (stat(path.c_str(), &node) != 0 ||
        !discovery_.shouldProbe(path.string(), node.st_rdev, node.st_ino)) continue;
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    const bool builtIn = isBuiltIn(fd);
    if (!builtIn && !isKeyboard(fd)) {
      discovery_.reject(path.string(), node.st_rdev, node.st_ino);
      close(fd);
      continue;
    }
    // Do not grab exclusively: APPLaunch watches the built-in hold-Esc shortcut.
    devices_.push_back({fd, path.string(), builtIn});
    state_.addDevice(fd, builtIn);
    SDL_Log("Zero %s keyboard connected: %s", builtIn ? "built-in" : "external", path.c_str());
  }
  if (!error) discovery_.endScan();
}

void KeyboardDevices::remove(std::size_t index) {
  const auto& device = devices_[index];
  SDL_Log("Zero keyboard disconnected: %s", device.path.c_str());
  state_.removeDevice(device.fd);
  close(device.fd);
  devices_.erase(devices_.begin() + index);
}

int KeyboardDevices::pollEvent(SDL_Event* event) {
  const Uint32 now = SDL_GetTicks();
  if (SDL_TICKS_PASSED(now, nextDiscovery_)) discover();
  // Drain queued releases before accepting new events, including reused evdev FDs.
  // Repeats run below, after checking physical releases/disconnections.
  if (state_.pollEvent(event, now, false)) return 1;
  for (std::size_t visited = 0; visited < devices_.size(); ++visited) {
    if (nextDevice_ >= devices_.size()) nextDevice_ = 0;
    const std::size_t index = nextDevice_++;
    auto& device = devices_[index];
    input_event input{};
    for (int count = 0; count < 64; ++count) {
      const ssize_t size = read(device.fd, &input, sizeof(input));
      if (size < 0 && errno == EINTR) continue;
      if (size != static_cast<ssize_t>(sizeof(input))) {
        if (size == 0 || (size < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
          remove(index);
          if (state_.pollEvent(event, now, false)) return 1;
        }
        break;
      }
      if (input.type == EV_SYN) {
        if (input.code == SYN_DROPPED) {
          // Lost events must never leave Draw, modifiers, or Escape stuck down.
          state_.removeDevice(device.fd);
          state_.addDevice(device.fd, device.builtIn);
          device.dropped = true;
          if (state_.pollEvent(event, now, false)) return 1;
        } else if (input.code == SYN_REPORT) device.dropped = false;
        continue;
      }
      if (device.dropped || input.type != EV_KEY || input.value < 0 || input.value > 2) continue;
      if (input.code == KEY_BRIGHTNESSDOWN || input.code == KEY_BRIGHTNESSUP) {
        if (input.value == 0) continue;
        const int percent = adjustBacklight(input.code == KEY_BRIGHTNESSUP);
        if (percent >= 0 && backlightEventType_ != static_cast<Uint32>(-1)) {
          *event = {};
          event->type = backlightEventType_;
          event->user.code = percent;
          return 1;
        }
        continue;
      }
      // Only the I2C keypad uses the Zero firmware's special Sym key codes.
      const char symbol = device.builtIn ? symbolForKey(input.code) : 0;
      const SDL_Scancode scan = symbol != 0
          ? SDL_GetScancodeFromKey(static_cast<SDL_Keycode>(symbol)) : scanCode(input.code);
      state_.key(device.fd, scan, input.value != 0, input.value == 2, now,
                 symbol == 0 ? SDLK_UNKNOWN : static_cast<SDL_Keycode>(symbol));
      if (state_.pollEvent(event, now, false)) return 1;
    }
  }
  return state_.pollEvent(event, now) ? 1 : 0;
}

void KeyboardDevices::wait(int timeoutMs) const {
  std::vector<pollfd> descriptors;
  for (const auto& device : devices_) descriptors.push_back({device.fd, POLLIN, 0});
  poll(descriptors.data(), descriptors.size(), timeoutMs);
}

}  // namespace bitmap16::Zero
