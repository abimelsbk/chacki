#include "pet_state.h"

#include <Arduino.h>

const char* moodName(Mood m) {
  switch (m) {
    case Mood::Idle: return "idle";
    case Mood::Listening: return "listening";
    case Mood::Thinking: return "thinking";
    case Mood::Talking: return "talking";
    case Mood::Happy: return "happy";
    case Mood::Sad: return "sad";
    case Mood::Sleeping: return "sleeping";
    case Mood::Angry: return "angry";
    case Mood::Curious: return "curious";
    case Mood::Error: return "error";
    case Mood::Excited: return "excited";
    case Mood::Focused: return "focused";
    default: return "?";
  }
}

void PetState::begin(uint32_t now) {
  lastActivity_ = now;
  nextSpontaneous_ = now + random(25000, 50000);
}

void PetState::apply(Mood m, uint32_t) { current_ = m; }

void PetState::setMood(Mood m, uint32_t durationMs) {
  uint32_t now = millis();
  if (durationMs == 0) {
    base_ = m;
    until_ = 0;
  } else {
    until_ = now + durationMs;
  }
  apply(m, now);
}

void PetState::clearTemporary() {
  until_ = 0;
  apply(base_, millis());
}

void PetState::sleep() {
  base_ = Mood::Sleeping;
  until_ = 0;
  apply(Mood::Sleeping, millis());
}

void PetState::wake(uint32_t now) {
  lastActivity_ = now;
  drowsy_ = 0.f;
  if (base_ == Mood::Sleeping) base_ = Mood::Idle;
  until_ = 0;
  apply(base_, now);
}

void PetState::activity(uint32_t now) {
  lastActivity_ = now;
  drowsy_ = 0.f;
}

void PetState::setSleepTiming(uint32_t drowsyMs, uint32_t sleepMs) {
  drowsyMs_ = drowsyMs;
  sleepMs_ = sleepMs > drowsyMs + 1000 ? sleepMs : drowsyMs + 1000;
}

void PetState::setSadBase(bool sad) {
  if (sad && base_ == Mood::Idle) base_ = Mood::Sad;
  else if (!sad && base_ == Mood::Sad) base_ = Mood::Idle;
  else return;
  if (until_ == 0) apply(base_, millis());
}

bool PetState::registerTap(uint32_t now) {
  tapTimes_[tapIdx_] = now;
  tapIdx_ = (tapIdx_ + 1) % 6;
  for (uint32_t t : tapTimes_) {
    if (t == 0 || now - t > 3000) return false;
  }
  for (uint32_t& t : tapTimes_) t = 0;
  return true;
}

bool PetState::update(uint32_t now) {
  if (until_ != 0 && (int32_t)(now - until_) >= 0) {
    until_ = 0;
    apply(base_, now);
  }

  uint32_t idleFor = now - lastActivity_;
  bool resting = current_ == Mood::Idle || current_ == Mood::Sad;
  if (resting) {
    drowsy_ = idleFor > drowsyMs_ ? (float)(idleFor - drowsyMs_) / (float)(sleepMs_ - drowsyMs_) : 0.f;
    if (drowsy_ > 1.f) drowsy_ = 1.f;
    if (idleFor > sleepMs_) {
      sleep();
      return false;
    }
    if ((int32_t)(now - nextSpontaneous_) >= 0) {
      nextSpontaneous_ = now + random(30000, 75000);
      return drowsy_ < 0.4f;
    }
  }
  return false;
}
