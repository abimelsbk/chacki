#pragma once
#include <stdint.h>

// Tamagotchi-style needs, 0..100. They decay over time, are saved to NVS
// every few minutes, and offline time is applied (gently) on the next boot.
class PetNeeds {
 public:
  enum class Low : uint8_t { None, Hungry, Tired, Bored };

  void begin(bool timeValid);
  void update(uint32_t now, bool sleeping);

  bool feed();  // false = already full
  void play();
  void pet();
  void interact();  // talking, games
  void focusDone();

  float food() const { return food_; }
  float energy() const { return energy_; }
  float fun() const { return fun_; }
  Low lowest() const;  // most urgent need below 25, or None
  float wellbeing() const { return (food_ + energy_ + fun_) / 300.f; }
  void save();

 private:
  static float clamp(float v) { return v < 0 ? 0 : (v > 100 ? 100 : v); }
  float food_ = 80, energy_ = 80, fun_ = 80;
  uint32_t lastTick_ = 0, lastSave_ = 0;
  bool dirty_ = false;
};
