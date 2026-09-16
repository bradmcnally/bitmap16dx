#pragma once

#include <algorithm>
#include <cstddef>

#include "core/sketch.h"

namespace bitmap16 {

// Slots hold the states on either side of the current document. Undo/redo
// swaps a slot with the document, sharing the same storage in both directions.
class SketchHistory {
 public:
  static constexpr std::size_t kCapacity = 16;
  enum class Action : uint8_t { Draw = 1, Erase = 2, Move = 4 };

  void clear() { cursor_ = size_ = 0; held_ = group_ = 0; }
  void endGroup() { group_ = 0; }
  void endGroup(Action action) {
    if (group_ == static_cast<uint8_t>(action)) endGroup();
  }
  void setHeldActions(bool draw, bool erase, bool move) {
    held_ = (draw ? 1 : 0) | (erase ? 2 : 0) | (move ? 4 : 0);
    if ((held_ & group_) == 0) endGroup();
  }
  bool canUndo() const { return cursor_ > 0; }
  bool canRedo() const { return cursor_ < size_; }

  void record(const Sketch& sketch) {
    endGroup();
    if (cursor_ == kCapacity) {
      for (std::size_t i = 1; i < kCapacity; ++i) {
        states_[i - 1] = states_[i];
      }
      --cursor_;
    }
    states_[cursor_++] = sketch;
    size_ = cursor_;  // A new edit discards the abandoned redo branch.
  }

  // Call only after checking that the operation will change the document.
  void recordEdit(const Sketch& sketch, Action action) {
    const auto kind = static_cast<uint8_t>(action);
    if (group_ == kind && (held_ & kind) != 0) return;
    record(sketch);
    if ((held_ & kind) != 0) group_ = kind;
  }

  bool undo(Sketch& sketch) {
    endGroup();
    if (!canUndo()) return false;
    std::swap(states_[--cursor_], sketch);
    return true;
  }

  bool redo(Sketch& sketch) {
    endGroup();
    if (!canRedo()) return false;
    std::swap(states_[cursor_++], sketch);
    return true;
  }

 private:
  Sketch states_[kCapacity];
  std::size_t cursor_ = 0;
  std::size_t size_ = 0;
  uint8_t held_ = 0;
  uint8_t group_ = 0;
};

}  // namespace bitmap16
