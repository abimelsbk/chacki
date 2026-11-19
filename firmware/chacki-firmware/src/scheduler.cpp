#include "scheduler.h"

void Scheduler::begin(uint32_t now) {
  lastWater_ = lastStand_ = lastCustom_ = now;
}

void Scheduler::snoozeAll(uint32_t now) { begin(now); }

bool Scheduler::isNight(const Settings& s, bool timeValid, const struct tm& lt) const {
  if (!s.nightOn || !timeValid || s.nightStart == s.nightEnd) return false;
  int h = lt.tm_hour;
  if (s.nightStart > s.nightEnd) return h >= s.nightStart || h < s.nightEnd;
  return h >= s.nightStart && h < s.nightEnd;
}

SchedEvent Scheduler::update(const Settings& s, bool timeValid, const struct tm& lt, uint32_t now, bool quiet) {
  // alarm has priority and ignores "quiet" (except it fires once per day)
  if (s.alarmOn && timeValid && lt.tm_hour == s.alarmHour && lt.tm_min == s.alarmMin &&
      lastAlarmYday_ != lt.tm_yday) {
    lastAlarmYday_ = lt.tm_yday;
    return SchedEvent::Alarm;
  }

  bool night = isNight(s, timeValid, lt);
  if (wasNight_ != -1 && night != (bool)wasNight_) {
    wasNight_ = night;
    return night ? SchedEvent::NightStart : SchedEvent::Morning;
  }
  wasNight_ = night;

  if (night || quiet) return SchedEvent::None;

  auto due = [&](uint16_t mins, uint32_t& last) {
    if (mins == 0) {
      last = now;
      return false;
    }
    if (now - last >= (uint32_t)mins * 60000UL) {
      last = now;
      return true;
    }
    return false;
  };
  if (due(s.waterMin, lastWater_)) return SchedEvent::Water;
  if (due(s.standMin, lastStand_)) return SchedEvent::Stand;
  if (s.customText.length() && due(s.customMin, lastCustom_)) return SchedEvent::Custom;
  return SchedEvent::None;
}

// ---------------------------------------------------------------- pomodoro --

void Pomodoro::startFocus(const Settings& s, uint32_t now) {
  phase_ = Phase::Focus;
  start_ = now;
  duration_ = (uint32_t)s.pomoFocus * 60000UL;
}

void Pomodoro::startBreak(const Settings& s, uint32_t now) {
  phase_ = Phase::Break;
  start_ = now;
  bool longBreak = sessions_ > 0 && sessions_ % 4 == 0;
  duration_ = (uint32_t)(longBreak ? s.pomoLong : s.pomoBreak) * 60000UL;
}

void Pomodoro::stop() { phase_ = Phase::Off; }

Pomodoro::Event Pomodoro::update(uint32_t now) {
  if (phase_ == Phase::Off || now - start_ < duration_) return Event::None;
  if (phase_ == Phase::Focus) {
    sessions_++;
    phase_ = Phase::Off;
    return Event::FocusDone;
  }
  phase_ = Phase::Off;
  return Event::BreakDone;
}

uint32_t Pomodoro::remainingMs(uint32_t now) const {
  if (phase_ == Phase::Off) return 0;
  uint32_t el = now - start_;
  return el >= duration_ ? 0 : duration_ - el;
}

float Pomodoro::progress(uint32_t now) const {
  if (phase_ == Phase::Off || duration_ == 0) return 0.f;
  float p = (float)(now - start_) / duration_;
  return p > 1.f ? 1.f : p;
}
