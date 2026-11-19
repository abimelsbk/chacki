#pragma once
#include <stdint.h>
#include <time.h>

#include "settings.h"

enum class SchedEvent : uint8_t { None, NightStart, Morning, Water, Stand, Custom, Alarm };

// Night mode, interval reminders and the daily alarm.
class Scheduler {
 public:
  void begin(uint32_t now);
  // `quiet` = don't fire reminders now (focus session, popup open...);
  // they fire as soon as it's no longer quiet.
  SchedEvent update(const Settings& s, bool timeValid, const struct tm& lt, uint32_t now, bool quiet);
  bool isNight(const Settings& s, bool timeValid, const struct tm& lt) const;
  void snoozeAll(uint32_t now);  // restart reminder intervals

 private:
  uint32_t lastWater_ = 0, lastStand_ = 0, lastCustom_ = 0;
  int lastAlarmYday_ = -1;
  int8_t wasNight_ = -1;
};

// Pomodoro focus timer.
class Pomodoro {
 public:
  enum class Phase : uint8_t { Off, Focus, Break };
  enum class Event : uint8_t { None, FocusDone, BreakDone };

  void startFocus(const Settings& s, uint32_t now);
  void startBreak(const Settings& s, uint32_t now);
  void stop();
  Event update(uint32_t now);

  Phase phase() const { return phase_; }
  bool active() const { return phase_ != Phase::Off; }
  uint32_t remainingMs(uint32_t now) const;
  float progress(uint32_t now) const;  // 0..1 elapsed
  uint8_t sessions() const { return sessions_; }

 private:
  Phase phase_ = Phase::Off;
  uint32_t start_ = 0, duration_ = 0;
  uint8_t sessions_ = 0;
};
