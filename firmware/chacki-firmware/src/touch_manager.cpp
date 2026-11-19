#include "touch_manager.h"

#include <M5Unified.h>
#include <stdlib.h>

void TouchManager::begin() {
  M5.Touch.setHoldThresh(550);
  M5.Touch.setFlickThresh(12);
}

TouchInfo TouchManager::update() {
  TouchInfo out;
  auto d = M5.Touch.getDetail();
  out.x = d.x;
  out.y = d.y;

  if (d.wasPressed()) {
    lastX_ = d.x;
    dir_ = 0;
    reversals_ = 0;
    stroked_ = false;
  }

  // Count horizontal direction reversals while the finger moves -> petting.
  if (d.isFlicking() || d.isDragging()) {
    int dx = d.x - lastX_;
    if (abs(dx) >= 10) {
      int nd = dx > 0 ? 1 : -1;
      if (dir_ != 0 && nd != dir_) reversals_++;
      dir_ = nd;
      lastX_ = d.x;
      if (reversals_ >= 2 && !stroked_) {
        stroked_ = true;
        out.event = TouchEvent::Stroke;
        return out;
      }
      if (reversals_ >= 6) {  // keep purring while petting continues
        reversals_ = 2;
        out.event = TouchEvent::Stroke;
        return out;
      }
    }
  }

  if (d.wasHold() && !stroked_) {
    holding_ = true;
    out.event = TouchEvent::HoldStart;
    return out;
  }

  if (d.wasReleased()) {
    if (holding_) {
      holding_ = false;
      out.event = TouchEvent::HoldEnd;
      return out;
    }
    if (stroked_) return out;
  }

  if (d.wasFlicked() && !stroked_) {
    int dx = d.x - d.base_x;
    int dy = d.y - d.base_y;
    if (abs(dx) > abs(dy)) {
      if (abs(dx) > 40) out.event = dx < 0 ? TouchEvent::SwipeLeft : TouchEvent::SwipeRight;
    } else if (abs(dy) > 40) {
      out.event = dy < 0 ? TouchEvent::SwipeUp : TouchEvent::SwipeDown;
    }
    return out;
  }

  if (d.wasClicked()) {
    out.event = d.getClickCount() >= 2 ? TouchEvent::DoubleTap : TouchEvent::Tap;
  }
  return out;
}
