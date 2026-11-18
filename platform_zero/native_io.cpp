#include "native_io.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>

#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace bitmap16::Zero {
namespace {

constexpr Uint32 kRepeatDelayMs = 260;
constexpr Uint32 kRepeatIntervalMs = 90;

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

int openKeypad() {
  // The kernel input name identifies the keypad even if event numbering changes.
  std::error_code error;
  std::filesystem::directory_iterator entry("/dev/input", error);
  for (const std::filesystem::directory_iterator end; !error && entry != end;
       entry.increment(error)) {
    const auto& path = entry->path();
    if (path.filename().string().compare(0, 5, "event") != 0) continue;
    const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;
    char name[128] = {};
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 &&
        (std::strstr(name, "tca8418") || std::strstr(name, "TCA8418"))) {
      return fd;
    }
    close(fd);
  }
  // Some images expose a generic keyboard name and a stable I2C path.
  return ::open("/dev/input/by-path/platform-3f804000.i2c-event",
                O_RDONLY | O_NONBLOCK | O_CLOEXEC);
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

}  // namespace

NativeIo::~NativeIo() {
  if (keyboard_ >= 0) close(keyboard_);
  if (mapping_) munmap(mapping_, mappingSize_);
  if (framebuffer_ >= 0) close(framebuffer_);
}

bool NativeIo::open() {
  framebuffer_ = ::open("/dev/fb_lcd", O_RDWR | O_CLOEXEC);
  if (framebuffer_ < 0 && errno == ENOENT) {
    framebuffer_ = ::open("/dev/fb0", O_RDWR | O_CLOEXEC);
  }
  if (framebuffer_ < 0 ||
      ioctl(framebuffer_, FBIOGET_FSCREENINFO, &fixed_) < 0 ||
      ioctl(framebuffer_, FBIOGET_VSCREENINFO, &mode_) < 0) {
    SDL_Log("Zero LCD open failed: %s", std::strerror(errno));
    return false;
  }

  const auto channel = [](const fb_bitfield& field, unsigned offset,
                          unsigned length) {
    return field.offset == offset && field.length == length &&
           field.msb_right == 0;
  };
  const std::size_t right = std::size_t{mode_.xoffset} + mode_.xres;
  const std::size_t bottom = std::size_t{mode_.yoffset} + mode_.yres;
  if (fixed_.type != FB_TYPE_PACKED_PIXELS ||
      fixed_.visual != FB_VISUAL_TRUECOLOR || mode_.bits_per_pixel != 16 ||
      !channel(mode_.red, 11, 5) || !channel(mode_.green, 5, 6) ||
      !channel(mode_.blue, 0, 5) || mode_.xres == 0 || mode_.yres == 0 ||
      fixed_.line_length == 0 || fixed_.line_length % sizeof(uint16_t) != 0 ||
      right > fixed_.line_length / sizeof(uint16_t) ||
      bottom > fixed_.smem_len / fixed_.line_length) {
    SDL_Log("Zero LCD has an unsupported framebuffer layout");
    return false;
  }

  mappingSize_ = fixed_.smem_len;
  void* mapping = mmap(nullptr, mappingSize_, PROT_READ | PROT_WRITE,
                       MAP_SHARED, framebuffer_, 0);
  if (mapping == MAP_FAILED) {
    SDL_Log("Zero LCD mapping failed: %s", std::strerror(errno));
    return false;
  }
  mapping_ = static_cast<uint8_t*>(mapping);

  keyboard_ = openKeypad();
  if (keyboard_ < 0) {
    SDL_Log("Zero keypad open failed: %s", std::strerror(errno));
    return false;
  }
  if (ioctl(keyboard_, EVIOCGRAB, 1UL) < 0) {
    SDL_Log("Zero keypad grab failed: %s", std::strerror(errno));
    return false;
  }
  backlightEventType_ = SDL_RegisterEvents(1);
  const char* directWrite = SDL_getenv("BITMAP16_FB_WRITE");
  directWrite_ = directWrite == nullptr || std::strcmp(directWrite, "0") != 0;
  SDL_Log("Zero LCD output: %s", directWrite_ ? "direct write" : "mapped");
  SDL_Log("Zero LCD: %ux%u RGB565, keypad acquired", mode_.xres, mode_.yres);
  return true;
}

void NativeIo::present(
    const uint16_t* pixels, int width, int height) {
  if (directWrite_) {
    if (static_cast<unsigned>(width) == mode_.xres &&
        static_cast<unsigned>(height) == mode_.yres &&
        mode_.xoffset == 0 &&
        fixed_.line_length == mode_.xres * sizeof(uint16_t)) {
      const auto* bytes = reinterpret_cast<const uint8_t*>(pixels);
      const std::size_t frameSize = std::size_t{mode_.yres} * fixed_.line_length;
      const auto origin =
          static_cast<off_t>(std::size_t{mode_.yoffset} * fixed_.line_length);
      std::size_t written = 0;
      while (written < frameSize) {
        const ssize_t count = pwrite(
            framebuffer_, bytes + written, frameSize - written,
            origin + static_cast<off_t>(written));
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) {
          SDL_Log("Zero LCD direct write failed: %s",
                  count < 0 ? std::strerror(errno) : "no bytes written");
          break;
        }
        written += static_cast<std::size_t>(count);
      }
      if (written == frameSize) return;
    } else {
      SDL_Log("Zero LCD direct write requires a tightly packed native frame");
    }
    SDL_Log("Zero LCD output: falling back to mapped");
    directWrite_ = false;
  }

  for (unsigned y = 0; y < mode_.yres; ++y) {
    const auto* source = pixels + (std::size_t{y} * height / mode_.yres) * width;
    auto* row = reinterpret_cast<uint16_t*>(
        mapping_ + (std::size_t{mode_.yoffset} + y) * fixed_.line_length) +
        mode_.xoffset;
    if (static_cast<unsigned>(width) == mode_.xres) {
      std::memcpy(row, source, mode_.xres * sizeof(*row));
      continue;
    }
    for (unsigned x = 0; x < mode_.xres; ++x) {
      row[x] = source[std::size_t{x} * width / mode_.xres];
    }
  }
}

int NativeIo::pollEvent(SDL_Event* event) {
  if (SDL_PollEvent(event)) return 1;
  input_event input{};
  constexpr auto eventSize = static_cast<ssize_t>(sizeof(input));
  while (read(keyboard_, &input, sizeof(input)) == eventSize) {
    if (input.type != EV_KEY) continue;
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
    const SDL_Scancode scan = scanCode(input.code);
    if (scan == SDL_SCANCODE_UNKNOWN) continue;

    const bool pressed = input.value != 0;
    const bool repeat = input.value == 2;
    const auto direction =
        std::find(kDirectionKeys.begin(), kDirectionKeys.end(), scan);
    if (direction != kDirectionKeys.end()) {
      // A draw key pressed after a direction can take over kernel autorepeat.
      // Repeat held directions independently of the other keypad keys.
      if (repeat) continue;
      if (pressed) {
        nextDirectionRepeat_[direction - kDirectionKeys.begin()] =
            SDL_GetTicks() + kRepeatDelayMs;
      }
    }
    keys_[scan] = pressed;
    fillKeyEvent(event, scan, pressed, repeat);
    return 1;
  }

  const Uint32 now = SDL_GetTicks();
  for (std::size_t i = 0; i < kDirectionKeys.size(); ++i) {
    const SDL_Scancode scan = kDirectionKeys[i];
    if (keys_[scan] && SDL_TICKS_PASSED(now, nextDirectionRepeat_[i])) {
      nextDirectionRepeat_[i] = now + kRepeatIntervalMs;
      fillKeyEvent(event, scan, true, true);
      return 1;
    }
  }
  return 0;
}

void NativeIo::fillKeyEvent(
    SDL_Event* event, SDL_Scancode scan, bool pressed, bool repeat) const {
  const Uint16 modifiers =
      (keys_[SDL_SCANCODE_LSHIFT] ? KMOD_LSHIFT : 0) |
      (keys_[SDL_SCANCODE_RSHIFT] ? KMOD_RSHIFT : 0) |
      (keys_[SDL_SCANCODE_LCTRL] ? KMOD_LCTRL : 0) |
      (keys_[SDL_SCANCODE_RCTRL] ? KMOD_RCTRL : 0) |
      (keys_[SDL_SCANCODE_LALT] ? KMOD_LALT : 0) |
      (keys_[SDL_SCANCODE_RALT] ? KMOD_RALT : 0);
  *event = {};
  event->type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
  event->key.timestamp = SDL_GetTicks();
  event->key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
  event->key.repeat = repeat;
  event->key.keysym = {scan, SDL_GetKeyFromScancode(scan), modifiers, 0};
}

int NativeIo::waitEvent(SDL_Event* event, int timeoutMs) {
  const Uint32 started = SDL_GetTicks();
  do {
    if (pollEvent(event)) return 1;
    // Keep servicing SDL's queue and signal handlers while waiting for evdev.
    int wait = 16;
    if (timeoutMs >= 0) {
      const auto elapsed = SDL_GetTicks() - started;
      if (elapsed >= static_cast<Uint32>(timeoutMs)) return 0;
      wait = std::min(wait, timeoutMs - static_cast<int>(elapsed));
    }
    pollfd descriptor{keyboard_, POLLIN, 0};
    const int result = poll(&descriptor, 1, wait);
    if ((result < 0 && errno != EINTR) ||
        (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL))) {
      SDL_Log("Zero keypad disconnected");
      *event = {};
      event->type = SDL_QUIT;
      return 1;
    }
  } while (true);
}

}  // namespace bitmap16::Zero
