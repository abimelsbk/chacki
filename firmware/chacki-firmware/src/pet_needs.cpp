#include "pet_needs.h"

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>

namespace {
// per-minute rates
constexpr float FOOD_DECAY = 0.33f;     // empty in ~5 h
constexpr float FUN_DECAY = 0.5f;       // empty in ~3.3 h
constexpr float ENERGY_DECAY = 0.25f;   // empty in ~6.7 h awake
constexpr float ENERGY_RECOVER = 1.5f;  // full in ~1 h asleep
constexpr uint32_t SAVE_EVERY_MS = 5UL * 60 * 1000;
}  // namespace

void PetNeeds::begin(bool timeValid) {
  Preferences p;
  if (p.begin("chacki-pet", true)) {
    food_ = p.getFloat("food", food_);
    energy_ = p.getFloat("energy", energy_);
    fun_ = p.getFloat("fun", fun_);
    uint32_t savedAt = p.getUInt("at", 0);
    p.end();
    // apply time spent switched off at a reduced rate (it was "resting")
    if (timeValid && savedAt > 1700000000) {
      uint32_t nowEpoch = (uint32_t)time(nullptr);
      if (nowEpoch > savedAt) {
        float mins = (nowEpoch - savedAt) / 60.f;
        if (mins > 720) mins = 720;
        food_ = clamp(food_ - FOOD_DECAY * 0.5f * mins);
        fun_ = clamp(fun_ - FUN_DECAY * 0.3f * mins);
        energy_ = clamp(energy_ + ENERGY_RECOVER * 0.5f * mins);
      }
    }
  }
  // never boot into a miserable pet
  if (food_ < 20) food_ = 20;
  if (fun_ < 20) fun_ = 20;
  lastTick_ = millis();
  lastSave_ = millis();
}

void PetNeeds::update(uint32_t now, bool sleeping) {
  uint32_t dtMs = now - lastTick_;
  if (dtMs < 1000) return;
  lastTick_ = now;
  float m = dtMs / 60000.f;
  food_ = clamp(food_ - FOOD_DECAY * m * (sleeping ? 0.5f : 1.f));
  fun_ = clamp(fun_ - FUN_DECAY * m * (sleeping ? 0.2f : 1.f));
  energy_ = clamp(energy_ + (sleeping ? ENERGY_RECOVER : -ENERGY_DECAY) * m);
  dirty_ = true;
  if (now - lastSave_ > SAVE_EVERY_MS) save();
}

bool PetNeeds::feed() {
  if (food_ > 92) return false;
  food_ = clamp(food_ + 30);
  fun_ = clamp(fun_ + 3);
  save();
  return true;
}

void PetNeeds::play() {
  fun_ = clamp(fun_ + 25);
  energy_ = clamp(energy_ - 6);
  food_ = clamp(food_ - 3);
  save();
}

void PetNeeds::pet() {
  fun_ = clamp(fun_ + 6);
  dirty_ = true;
}

void PetNeeds::interact() {
  fun_ = clamp(fun_ + 10);
  dirty_ = true;
}

void PetNeeds::focusDone() {
  fun_ = clamp(fun_ + 12);
  save();
}

PetNeeds::Low PetNeeds::lowest() const {
  float m = 25;
  Low l = Low::None;
  if (food_ < m) { m = food_; l = Low::Hungry; }
  if (energy_ < m) { m = energy_; l = Low::Tired; }
  if (fun_ < m) { l = Low::Bored; }
  return l;
}

void PetNeeds::save() {
  lastSave_ = millis();
  Preferences p;
  if (!p.begin("chacki-pet", false)) return;
  p.putFloat("food", food_);
  p.putFloat("energy", energy_);
  p.putFloat("fun", fun_);
  time_t t = time(nullptr);
  if (t > 1700000000) p.putUInt("at", (uint32_t)t);
  p.end();
  dirty_ = false;
}
