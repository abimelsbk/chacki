#pragma once
#include <Arduino.h>

// Snapshot of the pet published by the main loop for the web UI.
struct StatusSnapshot {
  char mood[12] = "idle";
  char personality[12] = "cheerful";
  float food = 0, energy = 0, fun = 0;
  int battery = -1;
  bool charging = false;
  bool sleeping = false;
  char ssid[33] = "";
  char ip[16] = "";
  int rssi = 0;
  char time[24] = "";
  uint32_t uptimeS = 0;
  char pomo[8] = "off";  // off | focus | break
  uint32_t pomoRemainS = 0;
  uint8_t sessions = 0;
  float ambient = 0;
};

enum class WebCmd : uint8_t {
  Feed, Play, Pet, Sleep, Wake, Mood, FocusStart, FocusStop,
  TestReminder, TestAlarm, TestSound, Dismiss, Launcher, Reboot, UpdatedFlash, UpdatedSd
};

struct WebCommand {
  WebCmd cmd;
  uint8_t arg;  // e.g. Mood index
};

// HTTP server on port 80 (http://<ip>/ or http://chacki.local/), protected by
// HTTP Basic auth (user "chacki", password from settings), plus OTA.
//
// OTA safety: an app can only update itself if there is a *second* app slot
// to write into. Under M5 Launcher there may be only one, and writing the
// running slot would brick Chacki. So:
//   flash mode - next OTA slot exists: browser upload + `pio run -t upload`
//                over Wi-Fi write straight to flash.
//   SD mode    - single slot: the upload is stored on the SD card as
//                /chacki/update.bin and Chacki reboots into Launcher for a
//                one-tap install.
class WebUi {
 public:
  enum class OtaMode : uint8_t { Flash, SdStage };

  void begin();
  void publish(const StatusSnapshot& s);
  bool takeCommand(WebCommand& out);

  OtaMode otaMode() const { return otaMode_; }
  bool otaActive() const { return otaActive_; }
  int otaProgress() const { return otaProgress_; }

  // Display freeze handshake for SD writes (the SD card shares the LCD's SPI bus).
  bool freezeRequested() const { return freezeReq_; }
  void ackFrozen() { frozen_ = true; }

 private:
  static void taskEntry(void* arg);
  void taskLoop();
  void setupRoutes();
  bool auth();
  void push(WebCmd c, uint8_t arg = 0);
  void handleUpload();
  void handleUploadDone();
  void startArduinoOta();
  bool freezeDisplay();
  void unfreezeDisplay();

  OtaMode otaMode_ = OtaMode::SdStage;
  volatile bool otaActive_ = false;
  volatile int otaProgress_ = 0;
  volatile bool freezeReq_ = false, frozen_ = false;

  // upload state (web task only)
  bool upReject_ = false, upOk_ = false;
  String upErr_;
  size_t upWritten_ = 0, upTotal_ = 0;
  uint8_t upPrevLast_ = 0;

  StatusSnapshot snap_;
  portMUX_TYPE mux_ = portMUX_INITIALIZER_UNLOCKED;
  void* queue_ = nullptr;
  bool started_ = false;
};

extern WebUi gWeb;
