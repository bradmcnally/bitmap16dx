#include "symbol_keys.h"
#include <cstdlib>
#include <iostream>

void require(bool value, const char* message) {
  if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
int main() {
  require(bitmap16::Zero::symbolForKey(71) == '-', "Sym+R enters a hyphen");
  require(bitmap16::Zero::symbolForKey(91) == '.', "Sym+B enters a period");
  require(bitmap16::Zero::symbolForKey(92) == '/', "Sym+N enters a slash");
  require(bitmap16::Zero::symbolForKey(80) == ':' &&
          bitmap16::Zero::symbolForKey(73) == '=' &&
          bitmap16::Zero::symbolForKey(93) == '?', "Symbols needed for URLs are mapped");
  require(bitmap16::Zero::symbolForKey(19) == 0 &&
          bitmap16::Zero::symbolForKey(48) == 0 &&
          bitmap16::Zero::symbolForKey(49) == 0 &&
          bitmap16::Zero::symbolForKey(1) == 0, "Plain letters and Escape retain normal handling");
}
