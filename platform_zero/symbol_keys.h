#pragma once

namespace bitmap16::Zero {
// Zero firmware uses dedicated evdev codes for its blue Sym legends.
// These codes do not have their usual PC keyboard meanings.
constexpr char symbolForKey(unsigned code) {
  switch (code) {
    case 26: return '!';
    case 27: return '@';
    case 39: return '#';
    case 40: return '$';
    case 41: return '%';
    case 43: return '^';
    case 51: return '&';
    case 52: return '*';
    case 53: return '(';
    case 94: return ')';
    case 55: return '~';
    case 69: return '`';
    case 70: return '_';
    case 71: return '-';
    case 72: return '+';
    case 73: return '=';
    case 74: return '[';
    case 75: return ']';
    case 76: return '{';
    case 77: return '}';
    case 79: return ';';
    case 80: return ':';
    case 81: return '\'';
    case 82: return '"';
    case 83: return '<';
    case 85: return '>';
    case 86: return '\\';
    case 89: return '|';
    case 90: return ',';
    case 91: return '.';
    case 92: return '/';
    case 93: return '?';
    default: return 0;
  }
}
}  // namespace bitmap16::Zero
