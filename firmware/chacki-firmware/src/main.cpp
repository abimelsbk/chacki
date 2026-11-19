// Chacki - cyberpunk AI desktop pet for M5Stack CoreS3-SE
//
// Touch
//   tap face ............ happy          tap an eye ....... "ouch"
//   double tap .......... curious        poke a lot ....... angry
//   rub back and forth .. petting        hold still ....... push-to-talk
//   swipe left / right .. pet card (food/energy/fun, FEED, PLAY, FOCUS)
//   swipe up ............ info card (time, web address + login, LAUNCHER)
//   swipe down .......... sleep          power button ..... sleep / wake
// Ears: clap = startled, double clap = wake / excited, talking nearby = curious.
// Web UI: http://<ip>/ or http://chacki.local/ (login shown on the info card).

#include <M5Unified.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "animation_engine.h"
#include "audio_manager.h"
#include "config.h"
#include "display_manager.h"
#include "network_manager.h"
#include "personality.h"
#include "pet_needs.h"
#include "pet_state.h"
#include "scheduler.h"
#include "settings.h"
#include "touch_manager.h"
#include "web_ui.h"

namespace {

DisplayManager display;
AnimationEngine anim;
AudioManager audio;
TouchManager touch;
NetworkManager net;
PetState pet;
PetNeeds needs;
Scheduler sched;
Pomodoro pomo;

StatusInfo status;
Scene scene;
uint32_t lastFrame = 0, lastStatus = 0, lastPublish = 0;
bool frozenShown = false;

// push-to-talk
enum class Talk : uint8_t { None, Listening, Thinking, Talking };
Talk talk = Talk::None;
uint32_t talkPhaseAt = 0;

// cards
Scene::Card card = Scene::Card::None;
uint32_t cardUntil = 0;

// pop-ups (reminders, alarm, pomodoro)
struct Popup {
  bool active = false;
  PopupIcon icon = PopupIcon::None;
  char title[24] = "";
  char text[72] = "";
  ButtonId b1 = ButtonId::None, b2 = ButtonId::None;
  uint32_t until = 0;      // auto-dismiss
  bool alarm = false;      // repeats the chime until dismissed
  bool sleepAfter = false;  // fall asleep when closed (good night)
  uint32_t nextChime = 0;
  uint8_t chimes = 0;
} popup;
uint32_t alarmSnoozeAt = 0;

// activities
enum class Act : uint8_t { None, Eating, Playing };
Act act = Act::None;
uint32_t actStart = 0;
float ballX = 0, ballY = 0, ballVX = 0, ballVY = 0;

// need bubbles
uint32_t nextBubble = 0, bubbleUntil = 0;
uint8_t bubbleKind = 0;

const Settings& S() { return gSettings.live(); }

bool isNightNow() { return sched.isNight(S(), status.timeValid, status.now); }

// ------------------------------------------------------------------ helpers --

void applySettings() {
  const Settings& s = S();
  const Traits& tr = traitsFor(s.personality);
  audio.setVolume(s.volume);
  audio.setSfxEnabled(s.sfx);
  audio.setPitch(tr.pitch);
  audio.setListening(s.clapOn, s.clapSens);
  anim.setTraits(tr);
  anim.setTheme(s.eyeColor, s.accent, s.eyeShape);
  display.setTheme(s.bgColor, s.accent, s.scanlines, tr.matrixRain);
  display.setName(s.name.c_str());
}

void updateSleepTiming() {
  const Traits& tr = traitsFor(S().personality);
  float k = tr.sleepMul;
  if (needs.energy() < 15) k *= 2.f;
  if (isNightNow()) pet.setSleepTiming(20000, 60000);
  else pet.setSleepTiming((uint32_t)(DROWSY_AFTER_MS / k), (uint32_t)(SLEEP_AFTER_MS / k));
}

void react(Mood m, uint32_t ms, Sfx s, const char* evt) {
  pet.setMood(m, ms);
  audio.play(s);
  anim.bounce();
  net.sendPetEvent(evt, moodName(m));
}

void showPopup(PopupIcon icon, const char* title, const char* text, ButtonId b1, ButtonId b2, uint32_t autoMs) {
  popup = Popup();
  popup.active = true;
  popup.icon = icon;
  strlcpy(popup.title, title, sizeof(popup.title));
  strlcpy(popup.text, text, sizeof(popup.text));
  popup.b1 = b1;
  popup.b2 = b2;
  popup.until = autoMs ? millis() + autoMs : 0;
  card = Scene::Card::None;
}

void closePopup() {
  bool sleepAfter = popup.sleepAfter;
  popup.active = false;
  if (sleepAfter) {
    pet.sleep();
    audio.play(Sfx::Sleep);
  }
}

void wakeIfSleeping(uint32_t now, bool sound = true) {
  if (!pet.sleeping()) return;
  pet.wake(now);
  if (sound) audio.play(Sfx::Wake);
}

void hardRestart(const char* title, const char* l1, const char* l2) {
  display.showMessage(title, l1, l2);
  needs.save();
  uint32_t t0 = millis();
  while (millis() - t0 < 1200) {
    audio.update();
    delay(5);
  }
  esp_restart();
}

// Hand control back to M5 Launcher (factory/test app slot), or just restart.
void exitToLauncher(const char* l1 = "Returning to M5 Launcher...", const char* l2 = "bye for now") {
  display.showMessage("LAUNCHER", l1, l2);
  needs.save();
  audio.play(Sfx::Sleep, true);
  uint32_t t0 = millis();
  while (millis() - t0 < 1500) {
    audio.update();
    delay(5);
  }
  const esp_partition_t* running = esp_ota_get_running_partition();
  const esp_partition_t* target =
      esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, nullptr);
  if (!target)
    target = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, nullptr);
  if (target && target != running) {
    if (esp_ota_set_boot_partition(target) != ESP_OK) log_w("could not select %s", target->label);
  } else {
    display.showMessage("LAUNCHER", "Restarting...", "touch the screen at boot to open Launcher");
    delay(1500);
  }
  esp_restart();
}

// ------------------------------------------------------------------ actions --

void startEating(uint32_t now) {
  wakeIfSleeping(now, false);
  if (!needs.feed()) {
    pet.setMood(Mood::Curious, 2000);
    showPopup(PopupIcon::None, "NO THANKS", phrase(S().personality, Phrase::Full), ButtonId::None, ButtonId::None,
              2500);
    return;
  }
  act = Act::Eating;
  actStart = now;
  card = Scene::Card::None;
  pet.setMood(Mood::Excited, 3000);
  net.sendPetEvent("feed", "excited");
}

void startPlaying(uint32_t now) {
  wakeIfSleeping(now, false);
  act = Act::Playing;
  actStart = now;
  card = Scene::Card::None;
  ballX = random(2) ? 40 : 280;
  ballY = 60;
  ballVX = ballX < 160 ? 230 : -230;
  ballVY = -120;
  needs.play();
  pet.setMood(Mood::Excited, 5000);
  audio.play(Sfx::Play);
  net.sendPetEvent("play", "excited");
}

void startFocus(uint32_t now) {
  wakeIfSleeping(now, false);
  pomo.startFocus(S(), now);
  pet.setMood(Mood::Focused);
  card = Scene::Card::None;
  audio.play(Sfx::FocusStart);
  showPopup(PopupIcon::Focus, "FOCUS", phrase(S().personality, Phrase::FocusStart), ButtonId::None, ButtonId::None,
            2500);
}

void stopFocus() {
  pomo.stop();
  pet.setMood(Mood::Idle);
  audio.play(Sfx::Done);
}

void showAlarm(uint32_t now) {
  wakeIfSleeping(now, false);
  showPopup(PopupIcon::Alarm, "ALARM", phrase(S().personality, Phrase::Alarm), ButtonId::Ok, ButtonId::Snooze, 0);
  popup.alarm = true;
  popup.nextChime = now;
  pet.setMood(Mood::Excited, 4000);
}

void showReminder(uint32_t now, PopupIcon icon, const char* title, const char* text) {
  wakeIfSleeping(now, false);
  showPopup(icon, title, text, ButtonId::Ok, ButtonId::None, 60000);
  popup.nextChime = now + 20000;  // one gentle repeat
  audio.play(Sfx::Reminder);
  pet.setMood(Mood::Curious, 3000);
}

// ------------------------------------------------------------------ talk -----

void startListening() {
  if (pet.sleeping()) pet.wake(millis());
  audio.play(Sfx::Listen);
  uint32_t t0 = millis();
  while (millis() - t0 < 120) {
    audio.update();
    delay(2);
  }
  if (!audio.startRecording()) {
    react(Mood::Error, 2000, Sfx::Error, "error");
    return;
  }
  talk = Talk::Listening;
  pet.setMood(Mood::Listening, 60000);
}

void stopListening(uint32_t now) {
  uint32_t ms = audio.stopRecording();
  pet.clearTemporary();
  if (ms < MIN_RECORD_MS) {
    talk = Talk::None;
    react(Mood::Curious, 1500, Sfx::Curious, "ptt_cancel");
    return;
  }
  // Phase 4: upload the recording to POST /api/v1/voice here.
  talk = Talk::Thinking;
  talkPhaseAt = now;
  pet.setMood(Mood::Thinking, 60000);
  net.sendPetEvent("ptt", "thinking");
}

void updateTalk(uint32_t now) {
  switch (talk) {
    case Talk::Listening:
      if (!audio.recording()) stopListening(now);
      break;
    case Talk::Thinking:
      if (now - talkPhaseAt > 700) {
        if (audio.playRecordingAsRobot()) {
          talk = Talk::Talking;
          pet.setMood(Mood::Talking, 60000);
        } else {
          talk = Talk::None;
          pet.clearTemporary();
        }
      }
      break;
    case Talk::Talking:
      if (!audio.speaking()) {
        talk = Talk::None;
        needs.interact();
        pet.setMood(Mood::Happy, 1500);
        audio.play(Sfx::Done);
      }
      break;
    default:
      break;
  }
}

// ------------------------------------------------------------------ input ----

void onButton(ButtonId b, uint32_t now) {
  switch (b) {
    case ButtonId::Launcher: exitToLauncher(); break;
    case ButtonId::Close: card = Scene::Card::None; audio.play(Sfx::Tap); break;
    case ButtonId::Feed: startEating(now); break;
    case ButtonId::Play: startPlaying(now); break;
    case ButtonId::Focus:
    case ButtonId::StartFocus: closePopup(); startFocus(now); break;
    case ButtonId::StopFocus: card = Scene::Card::None; stopFocus(); break;
    case ButtonId::Snooze:
      closePopup();
      alarmSnoozeAt = now + 5UL * 60 * 1000;
      audio.play(Sfx::Tap);
      break;
    case ButtonId::Ok:
      closePopup();
      audio.play(Sfx::Tap);
      break;
    default: break;
  }
}

void handleTouch(uint32_t now) {
  TouchInfo ti = touch.update();
  if (ti.event == TouchEvent::None) return;
  pet.activity(now);

  if (popup.active) {
    if (ti.event == TouchEvent::Tap || ti.event == TouchEvent::DoubleTap) {
      ButtonId b = display.hit(ti.x, ti.y);
      if (b != ButtonId::None) onButton(b, now);
      else if (!popup.alarm) closePopup();
    }
    return;
  }

  if (card != Scene::Card::None) {
    cardUntil = now + INFO_CARD_MS;
    if (ti.event == TouchEvent::Tap || ti.event == TouchEvent::DoubleTap) {
      ButtonId b = display.hit(ti.x, ti.y);
      if (b != ButtonId::None) onButton(b, now);
      else card = Scene::Card::None;
    } else if (ti.event == TouchEvent::SwipeDown) {
      card = Scene::Card::None;
    } else if (ti.event == TouchEvent::SwipeLeft || ti.event == TouchEvent::SwipeRight) {
      card = card == Scene::Card::Pet ? Scene::Card::Info : Scene::Card::Pet;
    }
    return;
  }

  if (talk == Talk::Listening) {
    if (ti.event == TouchEvent::HoldEnd) stopListening(now);
    return;
  }
  if (talk != Talk::None || act != Act::None) return;

  if (pet.sleeping() && ti.event != TouchEvent::HoldStart) {
    pet.wake(now);
    react(Mood::Curious, 1800, Sfx::Wake, "wake");
    return;
  }

  switch (ti.event) {
    case TouchEvent::Tap:
      if (pomo.phase() == Pomodoro::Phase::Focus) {
        anim.gesture(Gesture::Shh);
        audio.play(Sfx::Tap);
      } else if (pet.registerTap(now)) {
        react(Mood::Angry, 3000, Sfx::Angry, "annoyed");
      } else if (display.hitEye(ti.x, ti.y, anim)) {
        react(Mood::Angry, 1100, Sfx::Tap, "poke_eye");
      } else {
        react(Mood::Happy, 1800, Sfx::Happy, "tap");
      }
      break;
    case TouchEvent::DoubleTap:
      react(Mood::Curious, 2200, Sfx::Curious, "double_tap");
      break;
    case TouchEvent::Stroke:
      needs.pet();
      react(Mood::Happy, 2500, Sfx::Purr, "pet");
      break;
    case TouchEvent::HoldStart:
      startListening();
      break;
    case TouchEvent::SwipeLeft:
    case TouchEvent::SwipeRight:
      card = Scene::Card::Pet;
      cardUntil = now + INFO_CARD_MS;
      audio.play(Sfx::Swipe);
      break;
    case TouchEvent::SwipeUp:
      card = Scene::Card::Info;
      cardUntil = now + INFO_CARD_MS;
      audio.play(Sfx::Swipe);
      break;
    case TouchEvent::SwipeDown:
      if (pomo.active()) stopFocus();
      pet.sleep();
      audio.play(Sfx::Sleep);
      net.sendPetEvent("sleep", "sleeping");
      break;
    default:
      break;
  }
}

void handleEars(uint32_t now) {
  AudioEvent e = audio.takeEvent();
  if (e == AudioEvent::None) return;
  if (talk != Talk::None || act != Act::None || popup.active) return;

  if (pomo.phase() == Pomodoro::Phase::Focus) {
    if (e != AudioEvent::Voice) anim.gesture(Gesture::Shh);
    return;
  }
  switch (e) {
    case AudioEvent::Clap:
      if (pet.sleeping()) return;  // light sleeper, but not that light
      pet.activity(now);
      anim.gesture(Gesture::Startle);
      pet.setMood(Mood::Curious, 1500);
      audio.play(Sfx::Startle);
      net.sendPetEvent("clap", "curious");
      break;
    case AudioEvent::DoubleClap:
      pet.activity(now);
      if (pet.sleeping()) {
        pet.wake(now);
        react(Mood::Happy, 2000, Sfx::Wake, "double_clap");
      } else {
        react(Mood::Excited, 2000, Sfx::Happy, "double_clap");
      }
      break;
    case AudioEvent::Voice:
      if (pet.sleeping() || isNightNow()) return;
      pet.setMood(Mood::Curious, 2500);
      anim.bounce(0.5f);
      break;
    default:
      break;
  }
}

void handleWeb(uint32_t now) {
  WebCommand c;
  while (gWeb.takeCommand(c)) {
    pet.activity(now);
    switch (c.cmd) {
      case WebCmd::Feed: startEating(now); break;
      case WebCmd::Play: startPlaying(now); break;
      case WebCmd::Pet:
        wakeIfSleeping(now);
        needs.pet();
        react(Mood::Happy, 2500, Sfx::Purr, "pet");
        break;
      case WebCmd::Sleep:
        if (pomo.active()) pomo.stop();
        pet.sleep();
        audio.play(Sfx::Sleep);
        break;
      case WebCmd::Wake:
        if (pet.sleeping()) {
          pet.wake(now);
          react(Mood::Happy, 1500, Sfx::Wake, "wake");
        }
        break;
      case WebCmd::Mood: {
        Mood m = (Mood)c.arg;
        if (m == Mood::Sleeping) {
          pet.sleep();
          audio.play(Sfx::Sleep);
        } else {
          wakeIfSleeping(now, false);
          react(m, 4000, m == Mood::Sad ? Sfx::Sad : m == Mood::Angry ? Sfx::Angry
                         : m == Mood::Error ? Sfx::Error : Sfx::Happy, "web_mood");
        }
        break;
      }
      case WebCmd::FocusStart: startFocus(now); break;
      case WebCmd::FocusStop: if (pomo.active()) stopFocus(); break;
      case WebCmd::TestReminder:
        showReminder(now, PopupIcon::Water, "HYDRATE", phrase(S().personality, Phrase::Water));
        break;
      case WebCmd::TestAlarm: showAlarm(now); break;
      case WebCmd::TestSound: audio.play(Sfx::Happy, true); break;
      case WebCmd::Dismiss: if (popup.active) closePopup(); break;
      case WebCmd::Launcher: exitToLauncher(); break;
      case WebCmd::Reboot: hardRestart("REBOOT", "Restarting...", nullptr); break;
      case WebCmd::UpdatedFlash: hardRestart("UPDATED", "New firmware installed", "rebooting..."); break;
      case WebCmd::UpdatedSd:
        exitToLauncher("Update saved to SD card", "Launcher: SD > chacki > update.bin");
        break;
    }
  }
}

// ------------------------------------------------------------------ timers ---

void updateActivities(uint32_t now, float dt) {
  scene.foodY = -1;
  scene.ball = false;
  anim.setLookOverride(false);
  if (act == Act::Eating) {
    uint32_t el = now - actStart;
    if (el < 650) {
      scene.foodY = 30 + (175 - 30) * (el / 650.f);
      anim.setLookOverride(true, 0, -6 + 14 * (el / 650.f));
    } else if (anim.activeGesture() != Gesture::Chomp && el < 800) {
      anim.gesture(Gesture::Chomp);
      audio.play(Sfx::Chomp);
    } else if (el > 2300) {
      act = Act::None;
      react(Mood::Happy, 1800, Sfx::Happy, "fed");
    }
  } else if (act == Act::Playing) {
    uint32_t el = now - actStart;
    ballVY += 650.f * dt;
    ballX += ballVX * dt;
    ballY += ballVY * dt;
    if (ballY > 215) {
      ballY = 215;
      ballVY = -fabsf(ballVY) * 0.86f;
      if (fabsf(ballVY) < 180) ballVY = -random(280, 380);  // Chacki "headbutts" it back up
      audio.play(Sfx::Tap);
    }
    if (ballY < 34) { ballY = 34; ballVY = fabsf(ballVY); }
    if (ballX < 16) { ballX = 16; ballVX = fabsf(ballVX); }
    if (ballX > 304) { ballX = 304; ballVX = -fabsf(ballVX); }
    scene.ball = true;
    scene.ballX = ballX;
    scene.ballY = ballY;
    anim.setLookOverride(true, (ballX - 160) / 160.f * 24.f, (ballY - 120) / 100.f * 12.f);
    if (el > 4500) {
      act = Act::None;
      react(Mood::Happy, 1800, Sfx::Happy, "played");
    }
  }
}

void updateSchedule(uint32_t now) {
  bool quiet = popup.active || pomo.phase() == Pomodoro::Phase::Focus || talk != Talk::None || act != Act::None ||
               card != Scene::Card::None;
  const Personality pp = S().personality;

  if (alarmSnoozeAt && (int32_t)(now - alarmSnoozeAt) >= 0) {
    alarmSnoozeAt = 0;
    showAlarm(now);
  }

  switch (sched.update(S(), status.timeValid, status.now, now, quiet)) {
    case SchedEvent::Alarm: showAlarm(now); break;
    case SchedEvent::NightStart:
      if (!pomo.active() && talk == Talk::None) {
        showPopup(PopupIcon::Moon, "GOOD NIGHT", phrase(pp, Phrase::Night), ButtonId::None, ButtonId::None, 5000);
        popup.sleepAfter = true;
      }
      break;
    case SchedEvent::Morning:
      wakeIfSleeping(now);
      showPopup(PopupIcon::Sun, "GOOD MORNING", phrase(pp, Phrase::Morning), ButtonId::None, ButtonId::None, 6000);
      pet.setMood(Mood::Happy, 3000);
      break;
    case SchedEvent::Water: showReminder(now, PopupIcon::Water, "HYDRATE", phrase(pp, Phrase::Water)); break;
    case SchedEvent::Stand: showReminder(now, PopupIcon::Stand, "STAND UP", phrase(pp, Phrase::Stand)); break;
    case SchedEvent::Custom: showReminder(now, PopupIcon::Star, "REMINDER", S().customText.c_str()); break;
    default: break;
  }

  switch (pomo.update(now)) {
    case Pomodoro::Event::FocusDone:
      needs.focusDone();
      audio.play(Sfx::FocusDone, true);
      pomo.startBreak(S(), now);
      pet.setMood(Mood::Idle);
      pet.setMood(Mood::Happy, 3000);
      showPopup(PopupIcon::Star, "SESSION DONE", phrase(pp, Phrase::FocusDone), ButtonId::Ok, ButtonId::None, 30000);
      break;
    case Pomodoro::Event::BreakDone:
      audio.play(Sfx::Reminder, true);
      showPopup(PopupIcon::Focus, "BREAK OVER", phrase(pp, Phrase::BreakDone), ButtonId::StartFocus, ButtonId::Ok,
                120000);
      break;
    default: break;
  }
  if (pomo.phase() == Pomodoro::Phase::Focus) pet.activity(now);  // never doze off mid-session

  // popup timers
  if (popup.active) {
    if (popup.alarm) {
      if ((int32_t)(now - popup.nextChime) >= 0) {
        audio.play(Sfx::Alarm, true);
        popup.nextChime = now + 3000;
        if (++popup.chimes > 60) closePopup();  // give up after ~3 minutes
      }
    } else if (popup.nextChime && (int32_t)(now - popup.nextChime) >= 0) {
      popup.nextChime = 0;
      audio.play(Sfx::Reminder);
    }
    if (popup.until && (int32_t)(now - popup.until) >= 0) closePopup();
  }
}

void updateNeeds(uint32_t now) {
  needs.update(now, pet.sleeping());
  pet.setSadBase((needs.food() < 15 || needs.fun() < 15) && !pomo.active());

  PetNeeds::Low low = needs.lowest();
  bool idle = !pet.sleeping() && act == Act::None && talk == Talk::None && !popup.active && !pomo.active();
  if (low != PetNeeds::Low::None && idle && (int32_t)(now - nextBubble) >= 0) {
    bubbleKind = low == PetNeeds::Low::Hungry ? 1 : low == PetNeeds::Low::Tired ? 2 : 3;
    bubbleUntil = now + 4000;
    nextBubble = now + 25000;
    audio.play(Sfx::Curious);
  }
  scene.bubble = (now < bubbleUntil && idle) ? bubbleKind : 0;
}

void refreshStatus(uint32_t now) {
  if (now - lastStatus < 500 && lastStatus != 0) return;
  lastStatus = now;
  status.wifi = net.wifiState();
  status.rssi = net.rssi();
  status.backendConfigured = net.backendConfigured();
  status.backendOk = net.backendOk();
  status.battery = M5.Power.getBatteryLevel();
  status.charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
  status.timeValid = net.timeValid();
  if (status.timeValid) {
    time_t t = time(nullptr);
    localtime_r(&t, &status.now);
  }
  status.ssid = net.ssid();
  status.ip = net.ip();
  status.deviceId = gSettings.deviceId();
  status.webPass = gSettings.boot().webPass.c_str();
}

void publishSnapshot(uint32_t now) {
  if (now - lastPublish < 500) return;
  lastPublish = now;
  StatusSnapshot s;
  strlcpy(s.mood, moodName(pet.mood()), sizeof(s.mood));
  strlcpy(s.personality, personalityName(S().personality), sizeof(s.personality));
  s.food = needs.food();
  s.energy = needs.energy();
  s.fun = needs.fun();
  s.battery = status.battery;
  s.charging = status.charging;
  s.sleeping = pet.sleeping();
  strlcpy(s.ssid, net.ssid(), sizeof(s.ssid));
  strlcpy(s.ip, net.ip(), sizeof(s.ip));
  s.rssi = status.rssi;
  if (status.timeValid) strftime(s.time, sizeof(s.time), "%a %H:%M", &status.now);
  else strlcpy(s.time, "time not synced", sizeof(s.time));
  s.uptimeS = now / 1000;
  strlcpy(s.pomo, pomo.phase() == Pomodoro::Phase::Focus ? "focus" : pomo.phase() == Pomodoro::Phase::Break ? "break" : "off",
          sizeof(s.pomo));
  s.pomoRemainS = pomo.remainingMs(now) / 1000;
  s.sessions = pomo.sessions();
  s.ambient = audio.ambientLevel();
  gWeb.publish(s);
}

void buildScene(uint32_t now) {
  scene.card = popup.active ? Scene::Card::Popup : card;
  scene.icon = popup.icon;
  scene.title = popup.title;
  scene.text = popup.text;
  scene.btn1 = popup.b1;
  scene.btn2 = popup.b2;
  scene.pomoActive = pomo.active();
  scene.pomoBreak = pomo.phase() == Pomodoro::Phase::Break;
  scene.pomoProgress = pomo.progress(now);
  scene.pomoRemainS = pomo.remainingMs(now) / 1000;
  scene.pomoSessions = pomo.sessions();
  scene.food = needs.food();
  scene.energy = needs.energy();
  scene.fun = needs.fun();
  scene.ota = gWeb.otaActive();
  scene.otaProgress = gWeb.otaProgress();
}

}  // namespace

// ==================================================================== setup ==

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  cfg.internal_mic = true;
  cfg.internal_spk = true;
  cfg.clear_display = true;
  M5.begin(cfg);
  randomSeed(esp_random());

  // Settings + skins come from NVS / SD before the canvas uses the shared SPI bus.
  SettingsStore::Source src = gSettings.begin();
  net.restoreClockFromRtc();

  if (!display.begin()) {
    M5.Display.setTextColor(TFT_RED);
    M5.Display.drawString("PSRAM canvas alloc failed", 10, 10);
    for (;;) delay(1000);
  }
  bool audioOk = audio.begin(S().volume);
  applySettings();

  display.bootStep("initialising display", 0.f, 0.2f, 300);
  touch.begin();
  display.bootStep("touch online", 0.2f, 0.4f, 250);
  audio.play(Sfx::Boot);
  display.bootStep(audioOk ? "audio online" : "audio FAILED", 0.4f, 0.6f, 300);
  display.bootStep(src == SettingsStore::Source::SdCard ? "config imported from SD"
                   : src == SettingsStore::Source::Nvs  ? "config loaded"
                                                        : "no config - offline mode",
                   0.6f, 0.8f, 300);
  net.begin(gSettings.boot());
  if (gSettings.boot().wifiCount() > 0) gWeb.begin();
  display.bootStep("waking up...", 0.8f, 1.f, 400);

  needs.begin(net.timeValid());
  anim.begin();
  anim.setTraits(traitsFor(S().personality));
  pet.begin(millis());
  sched.begin(millis());
  if (!audioOk) pet.setMood(Mood::Error, 3000);
  lastFrame = millis();
}

// ===================================================================== loop ==

void loop() {
  M5.update();
  uint32_t now = millis();

  // An SD-staged firmware upload needs the SPI bus: stop drawing.
  if (gWeb.freezeRequested()) {
    if (!frozenShown) {
      display.showMessage("UPDATE", "Receiving firmware...", "saving to SD card");
      frozenShown = true;
      gWeb.ackFrozen();
    }
    audio.update();
    delay(5);
    return;
  }
  frozenShown = false;

  if (gSettings.syncLive()) applySettings();

  handleWeb(now);

  if (M5.BtnPWR.wasClicked()) {
    pet.activity(now);
    if (popup.active) closePopup();
    else if (pet.sleeping()) {
      pet.wake(now);
      react(Mood::Curious, 1500, Sfx::Wake, "wake");
    } else if (talk == Talk::None) {
      pet.sleep();
      audio.play(Sfx::Sleep);
    }
  }

  handleTouch(now);
  handleEars(now);
  updateTalk(now);
  audio.update();

  if (card != Scene::Card::None && (int32_t)(now - cardUntil) >= 0) card = Scene::Card::None;

  if (now - lastFrame >= FRAME_MS) {
    float dt = (now - lastFrame) / 1000.f;
    if (dt > 0.1f) dt = 0.1f;
    lastFrame = now;

    refreshStatus(now);
    updateSleepTiming();
    updateSchedule(now);
    updateNeeds(now);
    updateActivities(now, dt);
    if (pet.update(now) && act == Act::None && !popup.active) {
      Gesture q = traitsFor(S().personality).quirk;
      anim.gesture(q);
      if (q == Gesture::Wiggle) audio.play(Sfx::Happy);
      else if (q == Gesture::Scan) audio.play(Sfx::Bleep);
    }

    uint8_t bright = S().brightness;
    if (pet.sleeping()) bright = isNightNow() ? 8 : BRIGHTNESS_ASLEEP;
    if (popup.active && popup.alarm) bright = 255;
    display.setBrightness(bright);

    status.micLevel = audio.micLevel();
    float level = talk == Talk::Talking ? audio.speakLevel() : 0.f;
    anim.update(dt, now, pet.mood(), pet.drowsiness(), level);
    buildScene(now);
    display.render(anim, status, scene);
    publishSnapshot(now);
  } else {
    delay(1);
  }
}
