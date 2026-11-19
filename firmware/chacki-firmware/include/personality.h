#pragma once
#include <stdint.h>

#include "settings.h"

// Idle "quirks" that personalities use for spontaneous behaviour.
enum class Gesture : uint8_t { None, Wiggle, EyeRoll, Yawn, Scan, Startle, Shh, Chomp };

struct Traits {
  float lidBase;     // resting half-lid (0 = wide awake)
  float smileBias;   // added to mouth curve
  float blinkMul;    // >1 = blinks more often
  float lookMul;     // >1 = glances around more often
  float sleepMul;    // >1 = falls asleep sooner
  float pitch;       // sound-effect pitch multiplier
  bool matrixRain;   // hacker background
  Gesture quirk;     // signature spontaneous gesture
};

const Traits& traitsFor(Personality p);

enum class Phrase : uint8_t { Water, Stand, Alarm, FocusStart, FocusDone, BreakDone, Hungry, Tired, Bored, Morning, Night, Full };

// One-liners shown in pop-ups, picked per personality.
const char* phrase(Personality p, Phrase w);
