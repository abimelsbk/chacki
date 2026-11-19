#pragma once
#include <M5Unified.h>
#include <time.h>

#include "animation_engine.h"

struct StatusInfo {
  uint8_t wifi = 0;  // 0 = not configured, 1 = connecting, 2 = connected
  int rssi = 0;
  bool backendConfigured = false;
  bool backendOk = false;
  int battery = -1;
  bool charging = false;
  bool timeValid = false;
  struct tm now {};
  const char* ssid = "";
  const char* ip = "";
  const char* deviceId = "";
  const char* webPass = "";
  float micLevel = 0.f;
};

enum class ButtonId : uint8_t { None, Launcher, Close, Feed, Play, Focus, StopFocus, Ok, Snooze, StartFocus };

enum class PopupIcon : uint8_t { None, Water, Stand, Alarm, Focus, Star, Moon, Sun };

// Everything besides the face that should be drawn this frame.
struct Scene {
  enum class Card : uint8_t { None, Info, Pet, Popup };
  Card card = Card::None;

  // popup
  PopupIcon icon = PopupIcon::None;
  const char* title = "";
  const char* text = "";
  ButtonId btn1 = ButtonId::None, btn2 = ButtonId::None;

  // props
  float foodY = -1;  // falling data cube, <0 = none
  bool ball = false;
  float ballX = 0, ballY = 0;
  uint8_t bubble = 0;  // 0 none, 1 food, 2 energy, 3 fun

  // pomodoro
  bool pomoActive = false, pomoBreak = false;
  float pomoProgress = 0;
  uint32_t pomoRemainS = 0;
  uint8_t pomoSessions = 0;

  // needs
  float food = 0, energy = 0, fun = 0;

  // firmware update in progress
  bool ota = false;
  int otaProgress = 0;
};

class DisplayManager {
 public:
  bool begin();
  void setBrightness(uint8_t b);
  void setTheme(uint32_t bgRgb, uint32_t accentRgb, bool scanlines, bool matrixRain);
  void setName(const char* name);

  void bootStep(const char* label, float fromPct, float toPct, uint32_t ms);
  void render(const AnimationEngine& anim, const StatusInfo& st, const Scene& sc);
  void showMessage(const char* title, const char* line1, const char* line2 = nullptr);

  ButtonId hit(int x, int y) const;
  bool hitEye(int x, int y, const AnimationEngine& anim) const;

 private:
  struct Btn {
    int16_t x, y, w, h;
    ButtonId id;
  };
  void drawHud();
  void drawScanlines();
  void drawMatrix(float t);
  void drawEye(float ex, float ey, float w, float h, int side, const FaceParams& f, EyeShape shape, uint16_t col,
               float blink);
  void drawMouth(const AnimationEngine& anim, float mx, float my, uint16_t col, float micLevel);
  void drawExtras(const AnimationEngine& anim, float cx, float cy);
  void drawProps(const Scene& sc, float t);
  void drawStatusBar(const AnimationEngine& anim, const StatusInfo& st, const Scene& sc);
  void drawPomoBar(const Scene& sc);
  void drawInfoCard(const StatusInfo& st);
  void drawPetCard(const Scene& sc);
  void drawPopup(const Scene& sc, float t);
  void drawOta(const Scene& sc);
  void drawBootFrame(float pct, const char* label);
  void drawIcon(PopupIcon icon, int cx, int cy, float t);
  void addButton(int x, int y, int w, int h, ButtonId id, const char* label, uint16_t col);
  void panel(int x, int y, int w, int h);

  M5Canvas canvas_{&M5.Display};
  uint8_t brightness_ = 0;
  uint16_t bg_ = 0, accent_ = 0, panel_ = 0;
  bool scanlines_ = true, matrix_ = false;
  char name_[16] = "CHACKI";
  Btn btns_[4]{};
  uint8_t nBtns_ = 0;
  int16_t rainY_[22]{};
  uint8_t rainSpd_[22]{};
};
