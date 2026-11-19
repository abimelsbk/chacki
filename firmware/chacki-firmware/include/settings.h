#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

constexpr uint8_t MAX_WIFI_NETWORKS = 5;

enum class Personality : uint8_t { Cheerful, Sarcastic, Sleepy, Hacker, COUNT };
const char* personalityName(Personality p);
bool personalityFromName(const String& s, Personality& out);

enum class EyeShape : uint8_t { Round, Square, Pill, COUNT };
const char* eyeShapeName(EyeShape e);

struct Settings {
  // ---- network: applied after a reboot -------------------------------------
  String wifiSsid[MAX_WIFI_NETWORKS];
  String wifiPass[MAX_WIFI_NETWORKS];
  String backendUrl;
  String deviceToken;
  String tz = "IST-5:30";
  String webPass;  // web UI + OTA password (generated on first boot)

  // ---- live: applied immediately ---------------------------------------------
  String name = "CHACKI";
  Personality personality = Personality::Cheerful;
  String skinName = "Neon Cyan";
  uint32_t eyeColor = 0x00E6FF;  // RGB888
  uint32_t accent = 0xAA46FF;
  uint32_t bgColor = 0x040612;
  EyeShape eyeShape = EyeShape::Round;
  bool scanlines = true;
  uint8_t brightness = 170;

  uint8_t volume = 150;
  bool sfx = true;
  bool clapOn = true;
  uint8_t clapSens = 6;  // 1..10

  bool nightOn = true;
  uint8_t nightStart = 23, nightEnd = 7;
  bool alarmOn = false;
  uint8_t alarmHour = 7, alarmMin = 30;
  uint16_t waterMin = 60;  // 0 = off
  uint16_t standMin = 45;
  uint16_t customMin = 0;
  String customText;

  uint8_t pomoFocus = 25, pomoBreak = 5, pomoLong = 15;  // minutes

  uint8_t wifiCount() const {
    uint8_t n = 0;
    for (auto& s : wifiSsid)
      if (s.length()) n++;
    return n;
  }
};

// Persisted in NVS. /chacki/config.txt on the SD card is imported only when
// its contents change, so edits made in the web UI survive reboots.
//
// Threading: the web server (core 0) patches the stored copy under a mutex and
// raises a flag; the pet loop (core 1) calls syncLive() to pick changes up.
class SettingsStore {
 public:
  enum class Source : uint8_t { Defaults, Nvs, SdCard };
  Source begin();
  const char* deviceId() const { return deviceId_; }

  Settings& live() { return live_; }
  bool syncLive();

  String toJson();
  bool applyJson(const String& body, bool& needsReboot, String& err);
  String webPass();
  const Settings& boot() const { return boot_; }

 private:
  void loadNvs(Settings& s, bool& found);
  void saveNvs(const Settings& s);
  bool importSd(Settings& s);
  static void copyLiveFields(Settings& dst, const Settings& src);

  Settings stored_, live_, boot_;
  SemaphoreHandle_t mtx_ = nullptr;
  volatile bool dirty_ = false;
  char deviceId_[20] = "chacki";
};

extern SettingsStore gSettings;

uint16_t rgb888to565(uint32_t c);
uint32_t parseHexColor(const String& s, uint32_t fallback);
String hexColor(uint32_t c);
