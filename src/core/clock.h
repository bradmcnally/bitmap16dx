#pragma once

#include <cstdint>

namespace bitmap16 {
namespace Clock {

using Milliseconds = uint32_t;

inline Milliseconds elapsed(Milliseconds now, Milliseconds then) {
  return now - then;
}

inline bool hasElapsed(
    Milliseconds now,
    Milliseconds then,
    Milliseconds interval) {
  return elapsed(now, then) >= interval;
}

}  // namespace Clock
}  // namespace bitmap16
