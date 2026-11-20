#include "esc_hold.h"
#include <cstdlib>
#include <iostream>
#include <limits>

void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
  using bitmap16::Zero::EscHold;
  EscHold tap;
  tap.setPressed(true, 100);
  tap.update(400);
  require(tap.secondsRemaining(400) == 0, "Short taps do not show a countdown");
  tap.setPressed(false, 400);
  tap.update(5000);
  require(!tap.homeRequested(), "A tap never becomes HOME after release");
  EscHold hold;
  hold.setPressed(true, 100);
  require(hold.secondsRemaining(599) == 0 && hold.secondsRemaining(600) == 3,
          "Show the hint after half a second");
  hold.setPressed(true, 1000);
  require(hold.secondsRemaining(1100) == 2 && hold.secondsRemaining(2100) == 1,
          "Key repeat does not reset the countdown");
  hold.update(3099);
  require(!hold.homeRequested(), "Do not exit before three seconds");
  hold.update(3100);
  require(hold.homeRequested(), "Exit at three seconds");
  hold.setPressed(false, 3101);
  require(hold.homeRequested(), "A completed HOME request stays latched");
  EscHold cancelled;
  cancelled.setPressed(true, 100);
  cancelled.setPressed(false, 2500);
  require(cancelled.secondsRemaining(2500) == 0, "Release hides the hint");
  cancelled.setPressed(true, 2600);
  cancelled.update(3100);
  require(!cancelled.homeRequested() && cancelled.secondsRemaining(3100) == 3,
          "A new hold starts from three again");
  EscHold wrap;
  const uint32_t start = std::numeric_limits<uint32_t>::max() - 999u;
  wrap.setPressed(true, start);
  require(wrap.secondsRemaining(start + 1000u) == 2, "Countdown handles clock wrap");
  wrap.update(start + 3000u);
  require(wrap.homeRequested(), "HOME handles clock wrap");
}
