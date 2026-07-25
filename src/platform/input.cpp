#include "platform/input.h"

#include <M5Cardputer.h>

namespace {

bitmap16::InputProcessor processor;

}  // namespace

void Input::init() {
  processor.reset();
}

bitmap16::InputFrame Input::poll(uint32_t nowMs) {
  const Keyboard_Class::KeysState status =
      M5Cardputer.Keyboard.keysState();

  bitmap16::RawInputState raw;
  raw.keyChanged = M5Cardputer.Keyboard.isChange();
  raw.keyPressed = M5Cardputer.Keyboard.isPressed();
  raw.enter = status.enter;
  raw.deleteKey = status.del;
  raw.fn = status.fn;
  raw.tab = status.tab;
  raw.ctrl = status.ctrl;
  raw.actionButton = M5Cardputer.BtnA.isPressed();

  for (const char key : status.word) {
    if (key == '\0' || raw.keyCount >= bitmap16::RawInputState::kMaxKeys) {
      continue;
    }
    raw.keys[raw.keyCount++] = key;
  }

  return processor.process(raw, nowMs);
}

void Input::reset() {
  processor.reset();
}
