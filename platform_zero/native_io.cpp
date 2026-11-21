#include "native_io.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdio>

#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

namespace bitmap16::Zero {
NativeIo::~NativeIo() {
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

  backlightEventType_ = SDL_RegisterEvents(3);
  if (backlightEventType_ == static_cast<Uint32>(-1)) return false;
  homeEventType_ = backlightEventType_ + 1;
  keyboardLayoutEventType_ = backlightEventType_ + 2;
  if (!keyboards_.open(backlightEventType_)) {
    SDL_Log("Zero keyboard open failed: check /dev/input permissions");
    return false;
  }
  const char* directWrite = SDL_getenv("BITMAP16_FB_WRITE");
  directWrite_ = directWrite == nullptr || std::strcmp(directWrite, "0") != 0;
  SDL_Log("Zero LCD output: %s", directWrite_ ? "direct write" : "mapped");
  SDL_Log("Zero LCD: %ux%u RGB565, keyboards acquired", mode_.xres, mode_.yres);
  return true;
}

void NativeIo::present(const uint16_t* pixels, int width, int height) {
  if (escHold_.homeRequested()) return;
  frameWidth_ = width;
  frameHeight_ = height;
  lastFrame_.assign(pixels, pixels + std::size_t{static_cast<unsigned>(width)} * height);
  redrawEscCountdown();
}

void NativeIo::redrawEscCountdown() {
  if (lastFrame_.empty() || escHold_.homeRequested()) return;
  if (countdown_ == 0) {
    writeFrame(lastFrame_.data(), frameWidth_, frameHeight_);
    return;
  }
  if ((countdownFrame_.width() != frameWidth_ || countdownFrame_.height() != frameHeight_) &&
      !countdownFrame_.create(frameWidth_, frameHeight_)) return;
  std::copy(lastFrame_.begin(), lastFrame_.end(), countdownFrame_.pixels());
  // Match the regular centered status line, replacing the footer while held.
  const uint16_t background = countdownFrame_.readPixel(0, frameHeight_ - 1);
  const int red = ((background >> 11) & 31) * 255 / 31;
  const int green = ((background >> 5) & 63) * 255 / 63;
  const int blue = (background & 31) * 255 / 31;
  const uint16_t text = (red * 30 + green * 59 + blue * 11) / 100 >= 128
      ? 0x0000 : 0xffff;
  countdownFrame_.fillRect(0, frameHeight_ - 13, frameWidth_, 13, background);
  countdownFrame_.setTextColor(text);
  countdownFrame_.setTextSize(1);
  countdownFrame_.setTextAlign(TextAlign::Center);
  char label[24];
  std::snprintf(label, sizeof(label), "Exit in %d", countdown_);
  countdownFrame_.drawString(label, frameWidth_ / 2, frameHeight_ - 11);
  writeFrame(countdownFrame_.pixels(), frameWidth_, frameHeight_);
}

bool NativeIo::updateEsc(SDL_Event* event) {
  const Uint32 now = SDL_GetTicks();
  escHold_.update(now);
  if (escHold_.homeRequested()) {
    *event = {};
    event->type = homeEventType_;
    return true;
  }
  const int countdown = escHold_.secondsRemaining(now);
  if (countdown != countdown_) {
    countdown_ = countdown;
    redrawEscCountdown();
  }
  return false;
}

void NativeIo::writeFrame(
    const uint16_t* pixels, int width, int height) {
  pixels = warmthFilter_.apply(
      pixels, static_cast<std::size_t>(width) * height, displayWarmth_);
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
  const bool externalBefore = keyboards_.externalActive();
  const int available = keyboards_.pollEvent(event);
  escHold_.setPressed(keyboards_.keys()[SDL_SCANCODE_ESCAPE] != 0, SDL_GetTicks());
  if (updateEsc(event)) return 1;
  if (available) return 1;
  if (externalBefore != keyboards_.externalActive()) {
    *event = {};
    event->type = keyboardLayoutEventType_;
    return 1;
  }
  return SDL_PollEvent(event);
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
    keyboards_.wait(wait);
  } while (true);
}

}  // namespace bitmap16::Zero
