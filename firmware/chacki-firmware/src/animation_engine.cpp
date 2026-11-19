#include "animation_engine.h"

#include <Arduino.h>
#include <math.h>

namespace {

float approach(float cur, float target, float speed, float dt) {
  float k = speed * dt;
  if (k > 1.f) k = 1.f;
  return cur + (target - cur) * k;
}

float frand(float lo, float hi) { return lo + (hi - lo) * (float)random(0, 10001) / 10000.f; }

uint32_t gestureLength(Gesture g) {
  switch (g) {
    case Gesture::Wiggle: return 1000;
    case Gesture::EyeRoll: return 1300;
    case Gesture::Yawn: return 1800;
    case Gesture::Scan: return 1400;
    case Gesture::Startle: return 900;
    case Gesture::Shh: return 900;
    case Gesture::Chomp: return 1500;
    default: return 0;
  }
}

}  // namespace

void AnimationEngine::begin() {
  traits_ = traitsFor(Personality::Cheerful);
  cur_ = targetFor(Mood::Idle, 0.f);
  nextBlink_ = millis() + 1500;
  nextLook_ = millis() + 2000;
}

void AnimationEngine::setTheme(uint32_t eye, uint32_t acc, EyeShape shape) {
  skinR_ = (eye >> 16) & 0xFF;
  skinG_ = (eye >> 8) & 0xFF;
  skinB_ = eye & 0xFF;
  accR_ = (acc >> 16) & 0xFF;
  accG_ = (acc >> 8) & 0xFF;
  accB_ = acc & 0xFF;
  shape_ = shape;
}

void AnimationEngine::gesture(Gesture g) {
  gesture_ = g;
  gestureStart_ = millis();
  if (g == Gesture::Startle) bounce(1.3f);
}

void AnimationEngine::setLookOverride(bool on, float x, float y) {
  lookOverride_ = on;
  overX_ = x;
  overY_ = y;
}

FaceParams AnimationEngine::targetFor(Mood m, float drowsy) const {
  FaceParams p{};
  p.eyeW = 70; p.eyeH = 80; p.radius = 22; p.spacing = 70; p.scale = 1.f;
  p.mouthW = 34;
  p.mouthCurve = 0.3f + traits_.smileBias;
  p.lidTop = traits_.lidBase;
  p.r = skinR_; p.g = skinG_; p.b = skinB_;

  switch (m) {
    case Mood::Idle:
      p.lidTop = traits_.lidBase + (0.5f - traits_.lidBase * 0.5f) * drowsy;
      p.mouthCurve -= 0.3f * drowsy;
      break;
    case Mood::Listening:
      p.eyeW = 74; p.eyeH = 88; p.radius = 26; p.lidTop = 0;
      p.mouthCurve = 0.f;
      break;
    case Mood::Thinking:
      p.eyeW = 66; p.eyeH = 58; p.radius = 20; p.lidTop = 0.12f;
      p.mouthW = 22; p.mouthCurve = -0.15f;
      p.r = (skinR_ + accR_) / 2; p.g = (skinG_ + accG_) / 2; p.b = (skinB_ + accB_) / 2;
      break;
    case Mood::Talking:
      p.mouthW = 40; p.lidTop *= 0.5f;
      break;
    case Mood::Happy:
      p.eyeW = 76; p.lidBottom = 0.62f; p.lidTop = 0;
      p.mouthW = 44; p.mouthCurve = 1.f; p.blush = 1.f;
      break;
    case Mood::Excited:
      p.eyeW = 78; p.eyeH = 92; p.radius = 28; p.lidTop = 0;
      p.mouthW = 46; p.mouthCurve = 0.9f; p.mouthOpen = 0.35f; p.blush = 0.5f;
      break;
    case Mood::Focused:
      p.eyeW = 74; p.eyeH = 50; p.radius = 14;
      p.lidTop = 0.12f; p.lidTilt = 0.25f;
      p.mouthW = 20; p.mouthCurve = 0.f;
      break;
    case Mood::Sad:
      p.eyeW = 64; p.eyeH = 70; p.lidTop = 0.28f; p.lidTilt = -0.75f;
      p.mouthW = 30; p.mouthCurve = -0.8f;
      p.r = 60; p.g = 110; p.b = 255;
      break;
    case Mood::Sleeping:
      p.lidTop = 1.f; p.mouthW = 18; p.mouthCurve = 0.1f;
      p.r = skinR_ * 0.4f; p.g = skinG_ * 0.4f; p.b = skinB_ * 0.4f;
      break;
    case Mood::Angry:
      p.eyeW = 72; p.eyeH = 64; p.lidTop = 0.32f; p.lidTilt = 0.85f;
      p.mouthW = 32; p.mouthCurve = -0.6f;
      p.r = 255; p.g = 50; p.b = 90;
      break;
    case Mood::Curious:
      p.asym = 0.25f; p.lidTop = 0;
      p.mouthW = 16; p.mouthCurve = 0.f; p.mouthOpen = 0.2f;
      p.r = accR_; p.g = accG_; p.b = accB_;
      break;
    case Mood::Error:
      p.eyeW = 66; p.eyeH = 56; p.radius = 8;
      p.mouthW = 40; p.mouthCurve = -0.3f;
      p.r = 255; p.g = 30; p.b = 40;
      break;
    default:
      break;
  }

  // eye shape from the skin
  if (shape_ == EyeShape::Square) p.radius = fminf(p.radius, 7.f);
  else if (shape_ == EyeShape::Pill) p.radius = 60.f;  // clamped to half size when drawn
  return p;
}

void AnimationEngine::applyGesture(FaceParams& tgt, float& lx, float& ly, uint32_t now) {
  if (gesture_ == Gesture::None) return;
  uint32_t len = gestureLength(gesture_);
  float p = (float)(now - gestureStart_) / (float)len;
  if (p >= 1.f) {
    gesture_ = Gesture::None;
    return;
  }
  float bell = sinf(p * (float)M_PI);  // 0 -> 1 -> 0
  switch (gesture_) {
    case Gesture::Wiggle:
      lx = sinf(p * 6.f * (float)M_PI) * 14.f;
      tgt.mouthCurve = 0.9f;
      break;
    case Gesture::EyeRoll:
      lx = 18.f * cosf(p * (float)M_PI);
      ly = -12.f * sinf(p * (float)M_PI);
      tgt.lidTop += 0.2f * bell;
      tgt.mouthCurve = -0.2f;
      break;
    case Gesture::Yawn:
      tgt.mouthOpen = 0.9f * bell;
      tgt.mouthW = 30;
      tgt.lidTop += 0.55f * bell;
      ly = -4.f * bell;
      break;
    case Gesture::Scan:
      lx = sinf(p * 4.f * (float)M_PI) * 22.f;
      ly = 0;
      tgt.lidTop = 0.2f;
      break;
    case Gesture::Startle:
      tgt.scale = 1.f + 0.25f * bell;
      tgt.lidTop = 0;
      tgt.mouthOpen = 0.45f * bell;
      lx = 0;
      ly = -4.f * bell;
      break;
    case Gesture::Shh:
      tgt.lidTop += 0.3f;
      lx = 16.f * bell;
      tgt.mouthCurve = 0;
      break;
    case Gesture::Chomp:
      tgt.mouthOpen = fabsf(sinf(p * 7.f * (float)M_PI)) * 0.8f;
      tgt.lidBottom = 0.3f;
      break;
    default:
      break;
  }
}

void AnimationEngine::update(float dt, uint32_t now, Mood mood, float drowsy, float audioLevel) {
  t_ += dt;
  mood_ = mood;

  FaceParams tgt = targetFor(mood, drowsy);
  if (mood == Mood::Talking) tgt.mouthOpen = audioLevel > 0.04f ? 0.15f + audioLevel * 0.85f : 0.f;

  // --- gaze target -------------------------------------------------------
  switch (mood) {
    case Mood::Thinking:
      lookTX_ = 14.f + sinf(t_ * 0.9f) * 3.f;
      lookTY_ = -11.f;
      break;
    case Mood::Listening:
      lookTX_ = sinf(t_ * 0.7f) * 3.f;
      lookTY_ = -2.f;
      break;
    case Mood::Focused:
      lookTX_ = sinf(t_ * 0.3f) * 4.f;
      lookTY_ = 6.f;  // looking down at "work"
      break;
    case Mood::Sleeping:
      lookTX_ = 0;
      lookTY_ = 4.f;
      break;
    case Mood::Sad:
      lookTX_ = -4.f;
      lookTY_ = 9.f;
      break;
    default:
      if ((int32_t)(now - nextLook_) >= 0) {
        if (random(100) < 35) {
          lookTX_ = 0;
          lookTY_ = 0;
        } else {
          lookTX_ = frand(-20.f, 20.f);
          lookTY_ = frand(-10.f, 10.f);
        }
        float k = traits_.lookMul > 0.1f ? traits_.lookMul : 1.f;
        nextLook_ = now + (uint32_t)(random(1200, 4200) / k);
      }
      break;
  }
  float lx = lookTX_, ly = lookTY_;
  if (lookOverride_) {
    lx = overX_;
    ly = overY_;
  }
  applyGesture(tgt, lx, ly, now);
  if (tgt.lidTop > 1.f) tgt.lidTop = 1.f;

  // --- ease ---------------------------------------------------------------
  const float s = 9.f;
  cur_.eyeW = approach(cur_.eyeW, tgt.eyeW, s, dt);
  cur_.eyeH = approach(cur_.eyeH, tgt.eyeH, s, dt);
  cur_.radius = approach(cur_.radius, tgt.radius, s, dt);
  cur_.spacing = approach(cur_.spacing, tgt.spacing, s, dt);
  cur_.lidTop = approach(cur_.lidTop, tgt.lidTop, mood == Mood::Sleeping ? 2.5f : s, dt);
  cur_.lidTilt = approach(cur_.lidTilt, tgt.lidTilt, s, dt);
  cur_.lidBottom = approach(cur_.lidBottom, tgt.lidBottom, s, dt);
  cur_.asym = approach(cur_.asym, tgt.asym, s, dt);
  cur_.scale = approach(cur_.scale, tgt.scale, 18.f, dt);
  cur_.mouthW = approach(cur_.mouthW, tgt.mouthW, s, dt);
  cur_.mouthCurve = approach(cur_.mouthCurve, tgt.mouthCurve, s, dt);
  cur_.mouthOpen = approach(cur_.mouthOpen, tgt.mouthOpen, 30.f, dt);
  cur_.blush = approach(cur_.blush, tgt.blush, 4.f, dt);
  cur_.r = approach(cur_.r, tgt.r, 6.f, dt);
  cur_.g = approach(cur_.g, tgt.g, 6.f, dt);
  cur_.b = approach(cur_.b, tgt.b, 6.f, dt);

  // --- blinking ---------------------------------------------------------------
  const bool canBlink = mood != Mood::Sleeping && mood != Mood::Happy && gesture_ != Gesture::Yawn;
  if (canBlink && blinkStart_ == 0 && (int32_t)(now - nextBlink_) >= 0) blinkStart_ = now;
  if (blinkStart_ != 0) {
    float dur = traits_.blinkMul < 0.8f ? 230.f : 150.f;  // sleepy = slow blinks
    float ph = (now - blinkStart_) / dur;
    if (ph >= 1.f) {
      blink_ = 0.f;
      blinkStart_ = 0;
      if (!doubleBlink_ && random(100) < 18) {
        doubleBlink_ = true;
        nextBlink_ = now + 120;
      } else {
        doubleBlink_ = false;
        float k = traits_.blinkMul > 0.1f ? traits_.blinkMul : 1.f;
        nextBlink_ = now + (uint32_t)(random(2200, 6000) / k);
      }
    } else {
      blink_ = ph < 0.5f ? ph * 2.f : (1.f - ph) * 2.f;
    }
  } else {
    blink_ = 0.f;
  }

  float lookSpeed = (gesture_ != Gesture::None || lookOverride_) ? 22.f : 14.f;
  lookX_ = approach(lookX_, lx, lookSpeed, dt);
  lookY_ = approach(lookY_, ly, lookSpeed, dt);

  // --- breathing, bounce ----------------------------------------------------
  float rate = mood == Mood::Sleeping ? 0.9f : 1.7f;
  bob_ = sinf(t_ * rate) * (mood == Mood::Sleeping ? 3.f : 2.f);
  if (mood == Mood::Happy || mood == Mood::Excited) bob_ += sinf(t_ * 9.f) * 1.5f;
  if (gesture_ == Gesture::Wiggle) bob_ += sinf(t_ * 22.f) * 3.f;

  if (bounce_ > 0.f) {
    bounceT0_ = t_;
    bounceAmp_ = 9.f * bounce_;
    bounce_ = 0.f;
  }
  float bt = t_ - bounceT0_;
  bounceOff_ = (bt >= 0 && bt < 1.2f) ? -bounceAmp_ * sinf(bt * 18.f) * expf(-bt * 5.f) : 0.f;
}
