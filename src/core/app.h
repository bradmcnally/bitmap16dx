#pragma once

#include <cstdint>

namespace bitmap16 {

enum class ViewId : uint8_t {
  Canvas,
  Charging,
  Help,
  Memory,
  Preview,
  Palette,
  Settings,
};

class App {
 public:
  using FrameCallback = void (*)();

  bool init(FrameCallback frameCallback);
  void tick();

  bool isInitialized() const { return initialized_; }
  ViewId currentView() const { return currentView_; }
  ViewId previousView() const { return previousView_; }
  uint32_t frameCount() const { return frameCount_; }
  void setView(ViewId view);

 private:
  FrameCallback frameCallback_ = nullptr;
  ViewId currentView_ = ViewId::Canvas;
  ViewId previousView_ = ViewId::Canvas;
  uint32_t frameCount_ = 0;
  bool initialized_ = false;
};

}  // namespace bitmap16
