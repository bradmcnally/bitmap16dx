#pragma once

namespace bitmap16::Desktop {

enum class View { Canvas, Help, Settings, Preview, Charging, Palette, Memory };
enum class MenuCue { None, Open, Close, SketchOpen };

inline bool isMenu(View view) {
  return view == View::Help || view == View::Settings ||
         view == View::Palette || view == View::Memory;
}

inline MenuCue menuTransitionCue(View previous, View next, View helpReturn,
                                  bool cartridgeApplied = false, bool sketchOpened = false) {
  if (previous == next) return MenuCue::None;
  // Returning from Help closes its overlay rather than reopening its parent.
  if (previous == View::Help && next == helpReturn) return MenuCue::Close;
  if (isMenu(next)) return MenuCue::Open;
  if (previous == View::Memory && next == View::Canvas && sketchOpened)
    return MenuCue::SketchOpen;
  // Cartridge insertion already has its own success cue.
  if (previous == View::Palette && next == View::Canvas && cartridgeApplied)
    return MenuCue::None;
  return isMenu(previous) ? MenuCue::Close : MenuCue::None;
}

}  // namespace bitmap16::Desktop
