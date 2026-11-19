#pragma once
#include <stdint.h>

#include "personality.h"
#include "pet_state.h"
#include "settings.h"

// Every visual property of Chacki's face. The engine eases the current
// parameters toward a per-mood target each frame so changes morph smoothly.
struct FaceParams {
  float eyeW, eyeH, radius;
  float spacing;
  float lidTop;     // 0..1 covered from the top
  float lidTilt;    // + angry, - sad
  float lidBottom;  // happy "^ ^"
  float asym;       // curious: one eye bigger
  float scale;      // startle: whole-eye scale
  float mouthW;
  float mouthCurve;  // -1 .. +1
  float mouthOpen;   // 0..1
  float blush;
  float r, g, b;
};

class AnimationEngine {
 public:
  void begin();
  void update(float dt, uint32_t now, Mood mood, float drowsiness, float audioLevel);

  void setTheme(uint32_t eyeRgb, uint32_t accentRgb, EyeShape shape);
  void setTraits(const Traits& t) { traits_ = t; }
  void gesture(Gesture g);
  Gesture activeGesture() const { return gesture_; }
  void setLookOverride(bool on, float x = 0, float y = 0);
  void bounce(float amount = 1.f) { bounce_ = amount; }

  const FaceParams& face() const { return cur_; }
  Mood mood() const { return mood_; }
  EyeShape shape() const { return shape_; }
  float blink() const { return blink_; }
  float lookX() const { return lookX_; }
  float lookY() const { return lookY_; }
  float bob() const { return bob_; }
  float bounceOffset() const { return bounceOff_; }
  float time() const { return t_; }
  const Traits& traits() const { return traits_; }

 private:
  FaceParams targetFor(Mood m, float drowsiness) const;
  void applyGesture(FaceParams& tgt, float& lookTX, float& lookTY, uint32_t now);

  FaceParams cur_{};
  Mood mood_ = Mood::Idle;
  Traits traits_{};
  EyeShape shape_ = EyeShape::Round;
  float skinR_ = 0, skinG_ = 230, skinB_ = 255;
  float accR_ = 170, accG_ = 70, accB_ = 255;
  float t_ = 0.f;
  float blink_ = 0.f;
  uint32_t nextBlink_ = 0, blinkStart_ = 0;
  bool doubleBlink_ = false;
  float lookX_ = 0, lookY_ = 0, lookTX_ = 0, lookTY_ = 0;
  uint32_t nextLook_ = 0;
  bool lookOverride_ = false;
  float overX_ = 0, overY_ = 0;
  float bob_ = 0.f;
  Gesture gesture_ = Gesture::None;
  uint32_t gestureStart_ = 0;
  float bounce_ = 0.f, bounceOff_ = 0.f, bounceT0_ = -10.f, bounceAmp_ = 0.f;
};
