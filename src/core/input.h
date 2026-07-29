#pragma once

#include <cstddef>
#include <cstdint>

namespace bitmap16 {

enum class InputEvent : uint8_t {
  None,
  Up,
  Down,
  Left,
  Right,
  Enter,
  Delete,
  Space,
  Escape,
  Plus,
  Minus,
  Number0,
  Number1,
  Number2,
  Number3,
  Number4,
  Number5,
  Number6,
  Number7,
  Number8,
  Number9,
  Character,
};

struct RawInputState {
  static constexpr std::size_t kMaxKeys = 8;

  char keys[kMaxKeys] = {};
  uint8_t keyCount = 0;
  bool keyChanged = false;
  bool keyPressed = false;
  bool enter = false;
  bool deleteKey = false;
  bool fn = false;
  bool tab = false;
  bool ctrl = false;
  bool actionButton = false;
};

struct InputFrame {
  InputEvent event = InputEvent::None;
  char character = '\0';

  bool keyboardHeld = false;
  bool upHeld = false;
  bool downHeld = false;
  bool leftHeld = false;
  bool rightHeld = false;
  bool enterHeld = false;
  bool deleteHeld = false;
  bool fnHeld = false;
  bool tabHeld = false;
  bool ctrlHeld = false;
  bool mHeld = false;
  bool bHeld = false;
  bool lHeld = false;
  bool fHeld = false;
  bool actionHeld = false;

  bool enterPressed = false;
  bool deletePressed = false;
  bool ctrlPressed = false;
  bool actionPressed = false;
};

class InputProcessor {
 public:
  InputFrame process(const RawInputState& raw, uint32_t nowMs);
  void reset();

 private:
  static InputEvent directionEvent(const RawInputState& raw);
  static InputEvent keyEvent(char key, char& character);
  static bool containsKey(const RawInputState& raw, char lower, char upper);
  static bool isDirectionKey(char key);
  static bool isChordModifier(char key);

  InputEvent heldDirection_ = InputEvent::None;
  uint32_t lastDirectionTime_ = 0;
  bool directionRepeating_ = false;
  bool previousEnter_ = false;
  bool previousDelete_ = false;
  bool previousCtrl_ = false;
  bool previousAction_ = false;
};

}  // namespace bitmap16
