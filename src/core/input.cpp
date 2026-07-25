#include "core/input.h"

#include "core/clock.h"

namespace bitmap16 {

namespace {

constexpr uint32_t kRepeatDelayMs = 300;
constexpr uint32_t kRepeatRateMs = 100;

}  // namespace

InputFrame InputProcessor::process(
    const RawInputState& raw,
    uint32_t nowMs) {
  InputFrame frame;
  frame.keyboardHeld =
      raw.keyPressed || raw.keyCount > 0 || raw.enter || raw.deleteKey ||
      raw.fn || raw.tab || raw.ctrl;
  frame.enterHeld = raw.enter;
  frame.deleteHeld = raw.deleteKey;
  frame.fnHeld = raw.fn;
  frame.tabHeld = raw.tab;
  frame.ctrlHeld = raw.ctrl;
  frame.mHeld = containsKey(raw, 'm', 'M');
  frame.bHeld = containsKey(raw, 'b', 'B');
  frame.lHeld = containsKey(raw, 'l', 'L');
  frame.actionHeld = raw.actionButton;

  const InputEvent direction = directionEvent(raw);
  frame.upHeld = direction == InputEvent::Up;
  frame.downHeld = direction == InputEvent::Down;
  frame.leftHeld = direction == InputEvent::Left;
  frame.rightHeld = direction == InputEvent::Right;

  frame.enterPressed = raw.enter && !previousEnter_;
  frame.deletePressed = raw.deleteKey && !previousDelete_;
  frame.actionPressed = raw.actionButton && !previousAction_;
  previousEnter_ = raw.enter;
  previousDelete_ = raw.deleteKey;
  previousAction_ = raw.actionButton;

  if (direction != InputEvent::None) {
    if (direction != heldDirection_) {
      heldDirection_ = direction;
      lastDirectionTime_ = nowMs;
      directionRepeating_ = false;
      frame.event = direction;
    } else {
      const uint32_t threshold =
          directionRepeating_ ? kRepeatRateMs : kRepeatDelayMs;
      if (Clock::hasElapsed(nowMs, lastDirectionTime_, threshold)) {
        directionRepeating_ = true;
        lastDirectionTime_ = nowMs;
        frame.event = direction;
      }
    }
  } else {
    heldDirection_ = InputEvent::None;
    directionRepeating_ = false;
  }

  if (frame.event == InputEvent::None && raw.keyChanged && raw.keyPressed) {
    for (uint8_t i = 0; i < raw.keyCount; ++i) {
      const char key = raw.keys[i];
      if (isDirectionKey(key) || isChordModifier(key)) {
        continue;
      }

      frame.event = keyEvent(key, frame.character);
      if (frame.event != InputEvent::None) {
        break;
      }
    }
  }

  if (frame.event == InputEvent::None) {
    if (frame.enterPressed) {
      frame.event = InputEvent::Enter;
    } else if (frame.deletePressed) {
      frame.event = InputEvent::Delete;
    }
  }

  return frame;
}

void InputProcessor::reset() {
  heldDirection_ = InputEvent::None;
  lastDirectionTime_ = 0;
  directionRepeating_ = false;
  previousEnter_ = false;
  previousDelete_ = false;
  previousAction_ = false;
}

InputEvent InputProcessor::directionEvent(const RawInputState& raw) {
  for (uint8_t i = 0; i < raw.keyCount; ++i) {
    switch (raw.keys[i]) {
      case ';':
        return InputEvent::Up;
      case '.':
        return InputEvent::Down;
      case ',':
        return InputEvent::Left;
      case '/':
        return InputEvent::Right;
      default:
        break;
    }
  }
  return InputEvent::None;
}

InputEvent InputProcessor::keyEvent(char key, char& character) {
  switch (key) {
    case ' ':
      return InputEvent::Space;
    case '`':
      return InputEvent::Escape;
    case '+':
    case '=':
      return InputEvent::Plus;
    case '-':
    case '_':
      return InputEvent::Minus;
    case '0':
      return InputEvent::Number0;
    case '1':
      return InputEvent::Number1;
    case '2':
      return InputEvent::Number2;
    case '3':
      return InputEvent::Number3;
    case '4':
      return InputEvent::Number4;
    case '5':
      return InputEvent::Number5;
    case '6':
      return InputEvent::Number6;
    case '7':
      return InputEvent::Number7;
    case '8':
      return InputEvent::Number8;
    case '9':
      return InputEvent::Number9;
    default:
      if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) {
        character = key;
        return InputEvent::Character;
      }
      return InputEvent::None;
  }
}

bool InputProcessor::containsKey(
    const RawInputState& raw,
    char lower,
    char upper) {
  for (uint8_t i = 0; i < raw.keyCount; ++i) {
    if (raw.keys[i] == lower || raw.keys[i] == upper) {
      return true;
    }
  }
  return false;
}

bool InputProcessor::isDirectionKey(char key) {
  return key == ';' || key == '.' || key == ',' || key == '/';
}

bool InputProcessor::isChordModifier(char key) {
  return key == 'm' || key == 'M' ||
         key == 'b' || key == 'B' ||
         key == 'l' || key == 'L';
}

}  // namespace bitmap16
