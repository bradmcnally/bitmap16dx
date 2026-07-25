#include "platform/display.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <new>

#include "config.h"

namespace {

constexpr uint8_t kBacklightPin = 38;

bitmap16::Canvas* framebuffer = nullptr;

}  // namespace

bool Display::init() {
  if (framebuffer != nullptr) {
    return framebuffer->isValid();
  }

  framebuffer = new (std::nothrow) bitmap16::Canvas();
  if (framebuffer == nullptr ||
      !framebuffer->create(SCREEN_WIDTH, SCREEN_HEIGHT)) {
    delete framebuffer;
    framebuffer = nullptr;
    return false;
  }

  pinMode(kBacklightPin, OUTPUT);
  M5Cardputer.Display.fillScreen(0x0000);
  return true;
}

bool Display::isReady() {
  return framebuffer != nullptr && framebuffer->isValid();
}

bitmap16::Canvas& Display::canvas() {
  return *framebuffer;
}

void Display::beginFrame(uint16_t clearColor) {
  if (isReady()) {
    framebuffer->fillScreen(clearColor);
  }
}

bool Display::endFrame() {
  if (!isReady()) {
    return false;
  }
  M5Cardputer.Display.pushImage(
      0,
      0,
      framebuffer->width(),
      framebuffer->height(),
      framebuffer->pixels());
  return true;
}

void Display::setBrightness(uint8_t percent) {
  if (percent > 100) percent = 100;
  const uint8_t value =
      static_cast<uint8_t>((static_cast<uint16_t>(percent) * 255) / 100);
  const uint8_t mapped =
      static_cast<uint8_t>(160 + static_cast<uint16_t>(value) * 95 / 255);
  analogWrite(kBacklightPin, mapped);
}

void Display::shutdown() {
  delete framebuffer;
  framebuffer = nullptr;
}
