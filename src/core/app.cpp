#include "core/app.h"

namespace bitmap16 {

bool App::init(FrameCallback frameCallback) {
  frameCallback_ = frameCallback;
  frameCount_ = 0;
  currentView_ = ViewId::Canvas;
  previousView_ = ViewId::Canvas;
  initialized_ = frameCallback_ != nullptr;
  return initialized_;
}

void App::tick() {
  if (!initialized_) {
    return;
  }
  ++frameCount_;
  frameCallback_();
}

void App::setView(ViewId view) {
  if (view == currentView_) {
    return;
  }
  previousView_ = currentView_;
  currentView_ = view;
}

}  // namespace bitmap16
