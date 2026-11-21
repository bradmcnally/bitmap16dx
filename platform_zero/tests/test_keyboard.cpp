#include "keyboard_state.h"
#include "keyboard_event.h"
#include "esc_hold.h"

#include <cstdlib>
#include <iostream>

using bitmap16::Zero::KeyboardState;

namespace {
void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

SDL_Event next(KeyboardState& keyboard, Uint32 now = 0) {
  SDL_Event event{};
  require(keyboard.pollEvent(&event, now), "Expected a keyboard event");
  return event;
}

void drain(KeyboardState& keyboard) {
  SDL_Event event{};
  while (keyboard.pollEvent(&event, 0)) {}
}

void checkLayouts() {
  using bitmap16::Zero::isBuiltInKeyboardEvent;
  using bitmap16::Zero::navigationKey;
  KeyboardState keyboard;
  keyboard.addDevice(1, true);
  require(!keyboard.externalActive(), "Built-in keyboard starts with device hints");
  keyboard.addDevice(2, false);
  require(keyboard.externalActive(), "Connecting USB selects USB hints");
  constexpr SDL_Scancode letters[] = {
      SDL_SCANCODE_F, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C};
  constexpr SDL_Keycode arrows[] = {SDLK_UP, SDLK_LEFT, SDLK_DOWN, SDLK_RIGHT};
  for (int i = 0; i < 4; ++i) {
    keyboard.key(1, letters[i], true, false, 10);
    auto event = next(keyboard, 10);
    require(isBuiltInKeyboardEvent(event) && navigationKey(event) == arrows[i],
            "Built-in F/Z/X/C navigates in canvas, menus, and dialogs");
    require(!keyboard.externalActive(), "Using built-in keys selects device hints while USB remains connected");
    keyboard.key(1, letters[i], false, false, 11);
    next(keyboard, 11);
    keyboard.key(2, letters[i], true, false, 20);
    event = next(keyboard, 20);
    require(!isBuiltInKeyboardEvent(event) && navigationKey(event) == SDL_GetKeyFromScancode(letters[i]),
            "USB F/Z/X/C remains letters in canvas, menus, and dialogs");
    require(keyboard.externalActive(), "Using USB keys selects USB hints");
    require(!keyboard.pollEvent(&event, 400), "USB letters do not synthesize navigation repeats");
    keyboard.key(2, letters[i], true, true, 410);
    event = next(keyboard, 410);
    require(event.key.repeat && navigationKey(event) == SDL_GetKeyFromScancode(letters[i]),
            "USB letters retain kernel typing repeats without becoming arrows");
    keyboard.key(2, letters[i], false, false, 420);
    next(keyboard, 420);
  }
  keyboard.key(1, SDL_SCANCODE_F, true, false, 500);
  next(keyboard, 500);
  keyboard.key(2, SDL_SCANCODE_H, true, false, 510);
  next(keyboard, 510);
  auto event = next(keyboard, 760);
  require(isBuiltInKeyboardEvent(event) && navigationKey(event) == SDLK_UP,
          "Repeated built-in directions retain their source after USB input");
  require(keyboard.externalActive(), "Background repeats do not change the selected hint layout");
  keyboard.key(1, SDL_SCANCODE_F, false, false, 770);
  next(keyboard, 770);
  keyboard.key(1, SDL_SCANCODE_LCTRL, true, false, 780);
  next(keyboard, 780);
  keyboard.key(1, SDL_SCANCODE_Z, true, false, 790);
  event = next(keyboard, 790);
  require(navigationKey(event) == SDLK_z && (event.key.keysym.mod & KMOD_CTRL),
          "Built-in Ctrl+Z remains a command rather than navigation");
  keyboard.removeDevice(1);
  drain(keyboard);
  keyboard.key(2, SDL_SCANCODE_RIGHT, true, false, 800);
  require(navigationKey(next(keyboard, 800)) == SDLK_RIGHT,
          "Real USB arrows still navigate");
  keyboard.removeDevice(2);
  require(!keyboard.externalActive(), "Unplugging the final USB keyboard restores device hints");
}
}

int main() {
  SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
  require(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL dummy video initializes");
  checkLayouts();

  KeyboardState keyboard;
  keyboard.addDevice(1, true);
  keyboard.addDevice(2, false);
  keyboard.key(2, SDL_SCANCODE_RIGHT, true, false, 100);
  auto event = next(keyboard, 100);
  require(event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHT,
          "USB arrow produces a real arrow event");
  keyboard.key(2, SDL_SCANCODE_RETURN, true, false, 110);
  next(keyboard, 110);
  require(keyboard.keys()[SDL_SCANCODE_RETURN] && keyboard.keys()[SDL_SCANCODE_RIGHT],
          "Draw and arrow can be held together");
  require(!keyboard.pollEvent(&event, 359), "Direction waits for repeat delay");
  require(!keyboard.pollEvent(&event, 360, false),
          "Transport can drain pending events without synthesizing repeat before checking releases");
  event = next(keyboard, 360);
  require(event.key.keysym.sym == SDLK_RIGHT && event.key.repeat,
          "Arrow repeats while Draw owns kernel autorepeat");
  keyboard.key(2, SDL_SCANCODE_RIGHT, true, true, 365);
  require(!keyboard.pollEvent(&event, 365), "Kernel direction repeats are suppressed");

  keyboard.key(1, SDL_SCANCODE_RETURN, true, false, 370);
  keyboard.removeDevice(2);
  require(keyboard.keys()[SDL_SCANCODE_RETURN] && !keyboard.keys()[SDL_SCANCODE_RIGHT],
          "Unplug releases only that keyboard's held keys");
  event = next(keyboard, 370);
  require(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_RIGHT,
          "Unplug emits keyup for a released direction");
  require(!keyboard.pollEvent(&event, 1000), "Unplug stops arrow repeat without releasing other Draw");
  keyboard.key(1, SDL_SCANCODE_RETURN, false, false, 1000);
  event = next(keyboard, 1000);
  require(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_RETURN,
          "Final Draw release ends the stroke");

  keyboard.addDevice(2, false);
  keyboard.key(2, SDL_SCANCODE_DELETE, true, false, 1100);
  event = next(keyboard, 1100);
  require(event.key.keysym.sym == SDLK_BACKSPACE && keyboard.keys()[SDL_SCANCODE_BACKSPACE] &&
          !keyboard.keys()[SDL_SCANCODE_DELETE], "USB Delete erases rather than clearing artwork");
  keyboard.key(2, SDL_SCANCODE_BACKSPACE, true, false, 1101);
  keyboard.key(2, SDL_SCANCODE_DELETE, false, false, 1102);
  require(keyboard.keys()[SDL_SCANCODE_BACKSPACE], "Delete and Backspace retain separate physical states");
  keyboard.key(2, SDL_SCANCODE_BACKSPACE, false, false, 1103);
  next(keyboard, 1103);
  keyboard.key(1, SDL_SCANCODE_DELETE, true, false, 1104);
  require(next(keyboard, 1104).key.keysym.sym == SDLK_DELETE,
          "Built-in Fn+Backspace retains clear action");
  keyboard.key(1, SDL_SCANCODE_DELETE, false, false, 1105);
  drain(keyboard);

  keyboard.key(1, SDL_SCANCODE_LCTRL, true, false, 1200);
  next(keyboard, 1200);
  keyboard.key(2, SDL_SCANCODE_LSHIFT, true, false, 1201);
  next(keyboard, 1201);
  keyboard.key(2, SDL_SCANCODE_Z, true, false, 1202);
  event = next(keyboard, 1202);
  require(event.key.keysym.sym == SDLK_z && (event.key.keysym.mod & KMOD_CTRL) &&
          (event.key.keysym.mod & KMOD_SHIFT), "Ctrl+Shift+Z combines modifiers across devices");
  keyboard.removeDevice(2);
  require(keyboard.keys()[SDL_SCANCODE_LCTRL] && !keyboard.keys()[SDL_SCANCODE_LSHIFT] &&
          !keyboard.keys()[SDL_SCANCODE_Z], "Disconnect clears USB modifiers but retains built-in Ctrl");
  drain(keyboard);
  keyboard.removeDevice(1);
  drain(keyboard);

  keyboard.addDevice(3, false);
  keyboard.key(3, SDL_SCANCODE_KP_ENTER, true, false, 1300);
  event = next(keyboard, 1300);
  require(event.key.keysym.sym == SDLK_RETURN && keyboard.keys()[SDL_SCANCODE_RETURN],
          "Numpad Enter draws, confirms, and participates in held Draw");
  keyboard.removeDevice(3);
  drain(keyboard);
  keyboard.addDevice(3, false);
  keyboard.key(3, SDL_SCANCODE_LEFTBRACKET, true, false, 1400);
  require(next(keyboard, 1400).key.keysym.sym == SDLK_LEFTBRACKET,
          "USB punctuation retains standard layout");
  keyboard.addDevice(4, true);
  keyboard.key(4, SDL_SCANCODE_UNKNOWN, true, false, 1401, '!');
  require(next(keyboard, 1401).key.keysym.sym == '!', "Built-in Sym punctuation remains exact");
  keyboard.removeDevice(3);
  drain(keyboard);

  keyboard.addDevice(3, false);
  keyboard.key(3, SDL_SCANCODE_ESCAPE, true, false, 1500);
  next(keyboard, 1500);
  bitmap16::Zero::EscHold esc;
  esc.setPressed(keyboard.keys()[SDL_SCANCODE_ESCAPE], 1500);
  keyboard.removeDevice(3);
  esc.setPressed(keyboard.keys()[SDL_SCANCODE_ESCAPE], 1600);
  esc.update(5000);
  require(!esc.homeRequested(), "Unplug cancels a held-Escape exit countdown");
  drain(keyboard);
  keyboard.addDevice(3, false);
  keyboard.key(3, SDL_SCANCODE_LEFT, true, false, 5000);
  event = next(keyboard, 5000);
  require(event.type == SDL_KEYDOWN && !event.key.repeat,
          "Reconnecting the same device starts with fresh held-key state");
  keyboard.removeDevice(3);
  drain(keyboard);

  // SDL tick wraparound must preserve the delay rather than repeat immediately.
  keyboard.addDevice(5, false);
  keyboard.key(5, SDL_SCANCODE_UP, true, false, 0xffffff80u);
  next(keyboard, 0xffffff80u);
  require(!keyboard.pollEvent(&event, 0xffffff90u), "Repeat delay survives tick wraparound");
  require(keyboard.pollEvent(&event, 132u) && event.key.repeat,
          "Direction repeats after the wrapped deadline");
  SDL_Quit();
}
