#pragma once
#include <stdint.h>

enum class Mood : uint8_t {
  Idle,
  Listening,
  Thinking,
  Talking,
  Happy,
  Sad,
  Sleeping,
  Angry,
  Curious,
  Error,
  Excited,
  Focused,
  COUNT
};

const char* moodName(Mood m);

// Owns Chacki's emotional state: a "base" mood it falls back to, an optional
// temporary mood with an expiry, inactivity -> drowsy -> sleep, and a timer
// for spontaneous idle quirks (chosen by the caller per personality).
class PetState {
 public:
  void begin(uint32_t now);
  // Returns true when it's time for a spontaneous quirk.
  bool update(uint32_t now);

  void setMood(Mood m, uint32_t durationMs = 0);  // 0 = becomes the base mood
  void clearTemporary();
  void sleep();
  void wake(uint32_t now);
  void activity(uint32_t now);
  void setSleepTiming(uint32_t drowsyMs, uint32_t sleepMs);
  void setSadBase(bool sad);  // unmet needs make the resting mood sad

  bool registerTap(uint32_t now);  // true when poked too much

  Mood mood() const { return current_; }
  Mood base() const { return base_; }
  bool sleeping() const { return current_ == Mood::Sleeping; }
  bool hasTemporary() const { return until_ != 0; }
  float drowsiness() const { return drowsy_; }
  uint32_t idleMs(uint32_t now) const { return now - lastActivity_; }

 private:
  void apply(Mood m, uint32_t now);

  Mood base_ = Mood::Idle;
  Mood current_ = Mood::Idle;
  uint32_t until_ = 0;
  uint32_t lastActivity_ = 0;
  uint32_t nextSpontaneous_ = 0;
  uint32_t tapTimes_[6] = {0};
  uint8_t tapIdx_ = 0;
  float drowsy_ = 0.f;
  uint32_t drowsyMs_ = 90000, sleepMs_ = 180000;
};
