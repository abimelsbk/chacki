#pragma once
#include <stdint.h>

enum class TouchEvent : uint8_t {
  None,
  Tap,
  DoubleTap,
  HoldStart,  // finger held still -> push-to-talk begins
  HoldEnd,    // released after a hold
  SwipeLeft,
  SwipeRight,
  SwipeUp,
  SwipeDown,
  Stroke,  // rubbing back and forth = petting
};

struct TouchInfo {
  TouchEvent event = TouchEvent::None;
  int x = 0, y = 0;
};

// Turns M5Unified's raw touch states into pet-level gestures.
class TouchManager {
 public:
  void begin();
  TouchInfo update();  // call once per loop after M5.update()
  bool holding() const { return holding_; }

 private:
  bool holding_ = false;
  bool stroked_ = false;
  int lastX_ = 0;
  int dir_ = 0;
  uint8_t reversals_ = 0;
};
