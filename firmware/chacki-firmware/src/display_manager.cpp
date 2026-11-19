#include "display_manager.h"

#include <math.h>

#include "config.h"

namespace {

uint16_t scaled(float r, float g, float b, float f) {
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  return rgb565((uint8_t)(r * f), (uint8_t)(g * f), (uint8_t)(b * f));
}

uint16_t scale565(uint32_t rgb, float f) { return scaled((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, f); }

const char* WEEKDAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char* MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

const char* label(ButtonId id) {
  switch (id) {
    case ButtonId::Launcher: return "LAUNCHER";
    case ButtonId::Close: return "CLOSE";
    case ButtonId::Feed: return "FEED";
    case ButtonId::Play: return "PLAY";
    case ButtonId::Focus: return "FOCUS";
    case ButtonId::StopFocus: return "STOP";
    case ButtonId::Ok: return "OK";
    case ButtonId::Snooze: return "SNOOZE";
    case ButtonId::StartFocus: return "FOCUS";
    default: return "";
  }
}

}  // namespace

bool DisplayManager::begin() {
  // Rotation numbers differ between panels (CoreS3 vs CoreS3-SE): keep the
  // default and only flip if we ended up in portrait.
  if (M5.Display.width() < M5.Display.height()) {
    M5.Display.setRotation((M5.Display.getRotation() + 1) & 3);
  }
  setTheme(0x040612, 0xAA46FF, true, false);
  M5.Display.fillScreen(bg_);
  canvas_.setPsram(true);
  canvas_.setColorDepth(16);
  if (!canvas_.createSprite(SCREEN_W, SCREEN_H)) return false;
  for (int i = 0; i < 22; i++) {
    rainY_[i] = random(-240, 240);
    rainSpd_[i] = random(2, 7);
  }
  setBrightness(BRIGHTNESS_AWAKE);
  return true;
}

void DisplayManager::setBrightness(uint8_t b) {
  if (b == brightness_) return;
  brightness_ = b;
  M5.Display.setBrightness(b);
}

void DisplayManager::setTheme(uint32_t bg, uint32_t accent, bool scan, bool matrix) {
  bg_ = rgb888to565(bg);
  accent_ = rgb888to565(accent);
  // panels: background lifted a little toward the accent
  uint8_t r = ((bg >> 16) & 0xFF) + (((accent >> 16) & 0xFF) / 14) + 6;
  uint8_t g = ((bg >> 8) & 0xFF) + (((accent >> 8) & 0xFF) / 14) + 6;
  uint8_t b = (bg & 0xFF) + ((accent & 0xFF) / 14) + 12;
  panel_ = rgb565(r, g, b);
  scanlines_ = scan;
  matrix_ = matrix;
}

void DisplayManager::setName(const char* name) { strlcpy(name_, name, sizeof(name_)); }

// ---------------------------------------------------------------- helpers ---

void DisplayManager::drawHud() {
  const uint16_t c = accent_;
  const int L = 14, top = STATUS_BAR_H + 6, bot = SCREEN_H - 6, l = 6, r = SCREEN_W - 7;
  canvas_.drawFastHLine(l, top, L, c);
  canvas_.drawFastVLine(l, top, L, c);
  canvas_.drawFastHLine(r - L + 1, top, L, c);
  canvas_.drawFastVLine(r, top, L, c);
  canvas_.drawFastHLine(l, bot, L, c);
  canvas_.drawFastVLine(l, bot - L + 1, L, c);
  canvas_.drawFastHLine(r - L + 1, bot, L, c);
  canvas_.drawFastVLine(r, bot - L + 1, L, c);
}

void DisplayManager::drawScanlines() {
  if (!scanlines_) return;
  for (int y = STATUS_BAR_H; y < SCREEN_H; y += 3) canvas_.drawFastHLine(0, y, SCREEN_W, COL_SCAN);
}

void DisplayManager::drawMatrix(float t) {
  (void)t;
  canvas_.setFont(&fonts::Font0);
  canvas_.setTextDatum(top_left);
  for (int i = 0; i < 22; i++) {
    rainY_[i] += rainSpd_[i];
    if (rainY_[i] > SCREEN_H + 60) {
      rainY_[i] = random(-120, 0);
      rainSpd_[i] = random(2, 7);
    }
    int x = 4 + i * 15;
    for (int k = 0; k < 6; k++) {
      int y = rainY_[i] - k * 10;
      if (y < STATUS_BAR_H || y > SCREEN_H) continue;
      canvas_.setTextColor(scaled(40, 255, 110, k == 0 ? 0.55f : 0.30f - k * 0.045f));
      canvas_.drawChar((char)random(33, 126), x, y);
    }
  }
}

void DisplayManager::panel(int x, int y, int w, int h) {
  canvas_.fillRoundRect(x, y, w, h, 10, panel_);
  canvas_.drawRoundRect(x, y, w, h, 10, accent_);
}

void DisplayManager::addButton(int x, int y, int w, int h, ButtonId id, const char* text, uint16_t col) {
  canvas_.fillRoundRect(x, y, w, h, 8, bg_);
  canvas_.drawRoundRect(x, y, w, h, 8, col);
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextSize(1.f);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(COL_WHITE);
  canvas_.drawString(text, x + w / 2, y + h / 2);
  if (nBtns_ < 4) btns_[nBtns_++] = {(int16_t)x, (int16_t)y, (int16_t)w, (int16_t)h, id};
}

ButtonId DisplayManager::hit(int x, int y) const {
  for (uint8_t i = 0; i < nBtns_; i++) {
    const Btn& b = btns_[i];
    if (x >= b.x - 4 && x <= b.x + b.w + 4 && y >= b.y - 4 && y <= b.y + b.h + 4) return b.id;
  }
  return ButtonId::None;
}

bool DisplayManager::hitEye(int x, int y, const AnimationEngine& anim) const {
  const FaceParams& f = anim.face();
  float cy = 112 + anim.bob() + anim.lookY();
  for (int s = -1; s <= 1; s += 2) {
    float ex = 160 + s * f.spacing + anim.lookX();
    if (fabsf(x - ex) < f.eyeW / 2 + 8 && fabsf(y - cy) < f.eyeH / 2 + 8) return true;
  }
  return false;
}

// ---------------------------------------------------------------- boot ------

void DisplayManager::drawBootFrame(float pct, const char* lbl) {
  canvas_.fillScreen(bg_);
  drawHud();
  int jx = random(100) < 20 ? random(-4, 5) : 0;
  canvas_.setFont(&fonts::Orbitron_Light_32);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextSize(1.2f);
  canvas_.setTextColor(COL_PINK);
  canvas_.drawString(name_, 160 - 3 + jx, 92);
  canvas_.setTextColor(COL_CYAN);
  canvas_.drawString(name_, 160 + 3 - jx, 92);
  canvas_.setTextColor(COL_WHITE);
  canvas_.drawString(name_, 160, 92);
  canvas_.setTextSize(1.f);
  canvas_.setFont(&fonts::Font0);
  canvas_.setTextColor(accent_);
  canvas_.drawString("C Y B E R N E T I C   C O M P A N I O N", 160, 128);
  const int bx = 60, by = 152, bw = 200, bh = 8;
  canvas_.drawRect(bx - 2, by - 2, bw + 4, bh + 4, COL_DIM);
  int fill = (int)(bw * pct);
  canvas_.fillRect(bx, by, fill, bh, COL_CYAN);
  if (fill > 4) canvas_.fillRect(bx + fill - 4, by, 4, bh, COL_WHITE);
  canvas_.setTextColor(COL_DIM);
  canvas_.drawString(lbl, 160, 176);
  canvas_.drawString("v" CHACKI_VERSION, 160, 222);
  if (random(100) < 25) canvas_.fillRect(0, random(60, 200), SCREEN_W, random(1, 4), random(2) ? COL_PINK : COL_CYAN);
  drawScanlines();
  canvas_.pushSprite(0, 0);
}

void DisplayManager::bootStep(const char* lbl, float fromPct, float toPct, uint32_t ms) {
  uint32_t start = millis();
  for (;;) {
    float k = (float)(millis() - start) / (float)(ms ? ms : 1);
    if (k > 1.f) k = 1.f;
    drawBootFrame(fromPct + (toPct - fromPct) * k, lbl);
    if (k >= 1.f) break;
    delay(16);
  }
}

void DisplayManager::showMessage(const char* title, const char* line1, const char* line2) {
  canvas_.fillScreen(bg_);
  drawHud();
  canvas_.setTextDatum(middle_center);
  canvas_.setFont(&fonts::Orbitron_Light_24);
  canvas_.setTextSize(1.f);
  canvas_.setTextColor(COL_CYAN);
  canvas_.drawString(title, 160, 96);
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextColor(COL_WHITE);
  if (line1) canvas_.drawString(line1, 160, 140);
  canvas_.setTextColor(COL_DIM);
  if (line2) canvas_.drawString(line2, 160, 164);
  drawScanlines();
  canvas_.pushSprite(0, 0);
}

// ---------------------------------------------------------------- face ------

void DisplayManager::drawEye(float ex, float ey, float w, float h, int side, const FaceParams& f, EyeShape shape,
                             uint16_t col, float blink) {
  float he = h * (1.f - blink);
  if (he < 7.f || f.lidTop > 0.93f) {
    float px = ex - w / 2, py = ey;
    for (int i = 1; i <= 10; i++) {
      float u = i / 10.f * 2.f - 1.f;
      float x = ex + u * w / 2;
      float y = ey + 7.f * (1.f - u * u) - 3.f;
      canvas_.drawWideLine(px, py, x, y, 2.5f, col);
      px = x;
      py = y;
    }
    return;
  }

  int x0 = (int)(ex - w / 2), y0 = (int)(ey - he / 2);
  int wi = (int)w, hi = (int)he;
  int rad = (int)fminf(f.radius, fminf(he, w) / 2);

  for (int i = 3; i >= 1; i--) {
    int pad = i * 4;
    canvas_.fillRoundRect(x0 - pad, y0 - pad, wi + pad * 2, hi + pad * 2, rad + pad,
                          scaled(f.r, f.g, f.b, 0.07f * (4 - i)));
  }
  canvas_.fillSmoothRoundRect(x0, y0, wi, hi, rad, col);
  if (hi > 22) {
    canvas_.fillRoundRect(x0 + (int)(w * 0.16f), y0 + (int)(he * 0.12f), (int)(w * 0.22f),
                          (int)fmaxf(4.f, he * 0.18f), shape == EyeShape::Square ? 1 : 4, rgb565(210, 255, 255));
  }

  float cover = f.lidTop * he;
  float tilt = f.lidTilt * he * 0.35f;
  if (cover > 0.5f || fabsf(tilt) > 0.5f) {
    float yInner = y0 + cover + tilt, yOuter = y0 + cover - tilt;
    float yl = side < 0 ? yOuter : yInner, yr = side < 0 ? yInner : yOuter;
    int xl = x0 - 16, xr = x0 + wi + 16, top = y0 - 18;
    canvas_.fillTriangle(xl, top, xr, top, xr, (int)yr, bg_);
    canvas_.fillTriangle(xl, top, xr, (int)yr, xl, (int)yl, bg_);
  }
  if (f.lidBottom > 0.02f) {
    float ry = he * f.lidBottom;
    canvas_.fillEllipse((int)ex, (int)(y0 + he + ry * 0.25f + 4), (int)(w * 0.85f), (int)ry, bg_);
  }
}

void DisplayManager::drawMouth(const AnimationEngine& anim, float mx, float my, uint16_t col, float micLevel) {
  const FaceParams& f = anim.face();
  const float t = anim.time();
  if (anim.mood() == Mood::Listening) {
    for (int i = -5; i <= 5; i++) {
      float window = 1.f - fabsf((float)i) / 6.f;
      float wobble = 0.55f + 0.45f * sinf(t * 13.f + i * 0.9f);
      int h = 3 + (int)((4.f + micLevel * 40.f) * wobble * window);
      canvas_.fillRoundRect((int)mx + i * 9 - 2, (int)my - h / 2, 4, h, 2, col);
    }
    return;
  }
  if (f.mouthOpen > 0.06f) {
    int rx = (int)(f.mouthW * 0.35f + 4), ry = (int)(3 + f.mouthOpen * 13);
    canvas_.fillEllipse((int)mx, (int)my, rx + 3, ry + 3, scaled(f.r, f.g, f.b, 0.25f));
    canvas_.fillEllipse((int)mx, (int)my, rx, ry, col);
    if (ry > 6 && rx > 6) canvas_.fillEllipse((int)mx, (int)my, rx - 3, ry - 3, bg_);
    return;
  }
  float px = 0, py = 0;
  for (int i = 0; i <= 10; i++) {
    float u = i / 5.f - 1.f;
    float x = mx + u * f.mouthW / 2;
    float y = my + f.mouthCurve * 9.f * (1.f - u * u) - f.mouthCurve * 4.f;
    if (i) canvas_.drawWideLine(px, py, x, y, 2.5f, col);
    px = x;
    py = y;
  }
}

void DisplayManager::drawExtras(const AnimationEngine& anim, float cx, float cy) {
  const float t = anim.time();
  const FaceParams& f = anim.face();
  if (f.blush > 0.05f) {
    uint16_t pc = scaled(255, 60, 170, f.blush);
    for (int s = -1; s <= 1; s += 2) {
      float bx = cx + s * (f.spacing + 4) - 12, by = cy + 40;
      for (int k = 0; k < 3; k++) canvas_.drawWideLine(bx + k * 9, by + 6, bx + k * 9 + 6, by - 2, 1.5f, pc);
    }
  }
  switch (anim.mood()) {
    case Mood::Thinking:
      for (int i = 0; i < 3; i++) {
        float a = t * 4.f + i * 2.094f;
        canvas_.fillCircle((int)(cx + 112 + cosf(a) * 14), (int)(cy - 62 + sinf(a) * 14), 3 + i, accent_);
      }
      break;
    case Mood::Sleeping:
      canvas_.setFont(&fonts::Font4);
      canvas_.setTextDatum(middle_center);
      for (int k = 0; k < 3; k++) {
        float ph = fmodf(t * 0.35f + k / 3.f, 1.f);
        canvas_.setTextSize(0.6f + ph * 0.9f);
        canvas_.setTextColor(scaled(120, 140, 255, 1.f - ph));
        canvas_.drawString("z", (int)(cx + 95 + ph * 30 + sinf(t * 2 + k) * 4), (int)(cy - 30 - ph * 60));
      }
      canvas_.setTextSize(1.f);
      break;
    case Mood::Error:
      for (int i = 0; i < 4; i++) {
        int y = random(STATUS_BAR_H + 4, SCREEN_H - 4);
        canvas_.fillRect(random(0, 120), y, random(60, 240), random(1, 4), random(2) ? COL_RED : COL_CYAN);
      }
      canvas_.setFont(&fonts::Font0);
      canvas_.setTextDatum(middle_center);
      canvas_.setTextColor(COL_RED);
      canvas_.drawString("// SIGNAL LOST //", 160, SCREEN_H - 16);
      break;
    case Mood::Angry: {
      int ax = (int)(cx + 108), ay = (int)(cy - 66);
      for (int k = 0; k < 2; k++) {
        canvas_.drawWideLine(ax - 8, ay - 8 + k * 16, ax - 2, ay - 2 + k * 4, 2.f, COL_RED);
        canvas_.drawWideLine(ax + 8, ay - 8 + k * 16, ax + 2, ay - 2 + k * 4, 2.f, COL_RED);
      }
      break;
    }
    case Mood::Excited:
      for (int k = 0; k < 4; k++) {
        float a = t * 3.f + k * 1.57f;
        int sx = (int)(cx + cosf(a) * 130), sy = (int)(cy - 10 + sinf(a) * 70);
        canvas_.drawFastHLine(sx - 4, sy, 9, accent_);
        canvas_.drawFastVLine(sx, sy - 4, 9, accent_);
      }
      break;
    default:
      break;
  }
}

void DisplayManager::drawIcon(PopupIcon icon, int cx, int cy, float t) {
  uint16_t c = COL_CYAN;
  switch (icon) {
    case PopupIcon::Water:
      canvas_.fillCircle(cx, cy + 6, 11, c);
      canvas_.fillTriangle(cx - 10, cy + 3, cx + 10, cy + 3, cx, cy - 16, c);
      canvas_.fillCircle(cx - 4, cy + 4, 3, COL_WHITE);
      break;
    case PopupIcon::Stand: {
      int lift = (int)(fabsf(sinf(t * 4)) * 4);
      canvas_.fillCircle(cx, cy - 14 - lift, 5, c);
      canvas_.drawWideLine(cx, cy - 8 - lift, cx, cy + 6 - lift, 2.5f, c);
      canvas_.drawWideLine(cx, cy - 4 - lift, cx - 10, cy - 14 - lift, 2.f, c);
      canvas_.drawWideLine(cx, cy - 4 - lift, cx + 10, cy - 14 - lift, 2.f, c);
      canvas_.drawWideLine(cx, cy + 6 - lift, cx - 7, cy + 18, 2.f, c);
      canvas_.drawWideLine(cx, cy + 6 - lift, cx + 7, cy + 18, 2.f, c);
      break;
    }
    case PopupIcon::Alarm: {
      int sh = (int)(sinf(t * 30) * 3);
      canvas_.fillCircle(cx + sh, cy, 13, COL_AMBER);
      canvas_.fillRect(cx - 15 + sh, cy + 6, 30, 5, COL_AMBER);
      canvas_.fillCircle(cx + sh, cy + 14, 3, COL_AMBER);
      canvas_.fillCircle(cx + sh, cy - 15, 3, COL_AMBER);
      break;
    }
    case PopupIcon::Focus:
      canvas_.drawCircle(cx, cy, 15, c);
      canvas_.drawCircle(cx, cy, 9, c);
      canvas_.fillCircle(cx, cy, 4, COL_PINK);
      break;
    case PopupIcon::Star:
      for (int k = 0; k < 4; k++) {
        float a = k * 0.785f + t;
        canvas_.drawWideLine(cx - cosf(a) * 15, cy - sinf(a) * 15, cx + cosf(a) * 15, cy + sinf(a) * 15, 2.f,
                             accent_);
      }
      break;
    case PopupIcon::Moon:
      canvas_.fillCircle(cx, cy, 14, rgb565(200, 210, 255));
      canvas_.fillCircle(cx + 7, cy - 5, 12, panel_);
      break;
    case PopupIcon::Sun:
      canvas_.fillCircle(cx, cy, 9, COL_AMBER);
      for (int k = 0; k < 8; k++) {
        float a = k * 0.785f + t * 0.5f;
        canvas_.drawWideLine(cx + cosf(a) * 12, cy + sinf(a) * 12, cx + cosf(a) * 17, cy + sinf(a) * 17, 1.5f,
                             COL_AMBER);
      }
      break;
    default:
      break;
  }
}

void DisplayManager::drawProps(const Scene& sc, float t) {
  if (sc.foodY >= 0) {  // neon data cube
    int x = 160, y = (int)sc.foodY;
    canvas_.fillRect(x - 9, y - 9, 18, 18, COL_AMBER);
    canvas_.drawRect(x - 12, y - 12, 24, 24, scaled(255, 180, 40, 0.4f));
    canvas_.fillRect(x - 4, y - 4, 8, 8, COL_WHITE);
  }
  if (sc.ball) {
    canvas_.fillCircle((int)sc.ballX, (int)sc.ballY, 12, scaled(255, 60, 170, 0.3f));
    canvas_.fillCircle((int)sc.ballX, (int)sc.ballY, 9, COL_PINK);
    canvas_.fillCircle((int)sc.ballX - 3, (int)sc.ballY - 3, 3, COL_WHITE);
  }
  if (sc.bubble) {  // thought bubble, top-right
    int bx = 268, by = 52 + (int)(sinf(t * 2) * 2);
    canvas_.fillCircle(232, 86, 3, COL_WHITE);
    canvas_.fillCircle(244, 74, 5, COL_WHITE);
    canvas_.fillRoundRect(bx - 26, by - 20, 52, 40, 16, COL_WHITE);
    uint16_t ic = bg_;
    if (sc.bubble == 1) {  // food cube
      canvas_.fillRect(bx - 8, by - 8, 16, 16, COL_AMBER);
      canvas_.drawRect(bx - 8, by - 8, 16, 16, ic);
    } else if (sc.bubble == 2) {  // low battery
      canvas_.drawRect(bx - 13, by - 7, 24, 14, ic);
      canvas_.fillRect(bx + 11, by - 3, 3, 6, ic);
      canvas_.fillRect(bx - 11, by - 5, 5, 10, COL_RED);
    } else {  // ball
      canvas_.fillCircle(bx, by, 9, COL_PINK);
    }
  }
}

// ---------------------------------------------------------------- chrome ----

void DisplayManager::drawStatusBar(const AnimationEngine& anim, const StatusInfo& st, const Scene& sc) {
  canvas_.fillRect(0, 0, SCREEN_W, STATUS_BAR_H, panel_);
  canvas_.drawFastHLine(0, STATUS_BAR_H - 1, SCREEN_W, accent_);
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextSize(1.f);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(COL_CYAN);
  char buf[24];
  if (st.timeValid) snprintf(buf, sizeof(buf), "%02d:%02d", st.now.tm_hour, st.now.tm_min);
  else snprintf(buf, sizeof(buf), "--:--");
  canvas_.drawString(buf, 6, STATUS_BAR_H / 2);

  canvas_.setFont(&fonts::Font0);
  if (sc.pomoActive) {
    snprintf(buf, sizeof(buf), "%s %02lu:%02lu", sc.pomoBreak ? "BREAK" : "FOCUS",
             (unsigned long)(sc.pomoRemainS / 60), (unsigned long)(sc.pomoRemainS % 60));
    canvas_.setTextColor(sc.pomoBreak ? COL_GREEN : COL_PINK);
    canvas_.drawString(buf, 52, STATUS_BAR_H / 2);
  } else {
    canvas_.setTextColor(accent_);
    canvas_.drawString(moodName(anim.mood()), 52, STATUS_BAR_H / 2);
  }

  int bx = SCREEN_W - 30, by = 5;
  canvas_.drawRect(bx, by, 22, 10, COL_DIM);
  canvas_.fillRect(bx + 22, by + 3, 2, 4, COL_DIM);
  if (st.battery >= 0) {
    uint16_t bc = st.battery > 40 ? COL_GREEN : (st.battery > 15 ? COL_AMBER : COL_RED);
    canvas_.fillRect(bx + 2, by + 2, (18 * st.battery) / 100, 6, bc);
    canvas_.setTextDatum(middle_right);
    canvas_.setTextColor(st.charging ? COL_AMBER : COL_DIM);
    snprintf(buf, sizeof(buf), st.charging ? "+%d%%" : "%d%%", st.battery);
    canvas_.drawString(buf, bx - 4, STATUS_BAR_H / 2);
  }

  int wx = SCREEN_W - 92;
  int bars = 0;
  if (st.wifi == 2) bars = st.rssi > -55 ? 4 : st.rssi > -65 ? 3 : st.rssi > -75 ? 2 : 1;
  bool blinkOn = (millis() / 400) % 2;
  for (int i = 0; i < 4; i++) {
    int h = 3 + i * 3;
    uint16_t c = rgb565(25, 30, 50);
    if (st.wifi == 2 && i < bars) c = COL_CYAN;
    else if (st.wifi == 1 && blinkOn) c = COL_DIM;
    canvas_.fillRect(wx + i * 5, 15 - h, 3, h, c);
  }
  uint16_t dc = !st.backendConfigured ? rgb565(25, 30, 50) : (st.backendOk ? COL_GREEN : COL_RED);
  canvas_.fillCircle(wx - 8, STATUS_BAR_H / 2, 3, dc);
}

void DisplayManager::drawPomoBar(const Scene& sc) {
  if (!sc.pomoActive) return;
  int w = (int)(SCREEN_W * sc.pomoProgress);
  canvas_.fillRect(0, SCREEN_H - 3, SCREEN_W, 3, rgb565(20, 20, 40));
  canvas_.fillRect(0, SCREEN_H - 3, w, 3, sc.pomoBreak ? COL_GREEN : COL_PINK);
}

void DisplayManager::drawInfoCard(const StatusInfo& st) {
  panel(16, 26, 288, 208);
  char buf[64];
  canvas_.setTextDatum(middle_center);
  canvas_.setFont(&fonts::Orbitron_Light_24);
  canvas_.setTextSize(1.f);
  canvas_.setTextColor(COL_CYAN);
  if (st.timeValid) snprintf(buf, sizeof(buf), "%02d:%02d:%02d", st.now.tm_hour, st.now.tm_min, st.now.tm_sec);
  else snprintf(buf, sizeof(buf), "--:--:--");
  canvas_.drawString(buf, 160, 46);
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextColor(accent_);
  if (st.timeValid) {
    snprintf(buf, sizeof(buf), "%s %d %s %d", WEEKDAYS[st.now.tm_wday % 7], st.now.tm_mday,
             MONTHS[st.now.tm_mon % 12], st.now.tm_year + 1900);
    canvas_.drawString(buf, 160, 68);
  } else {
    canvas_.drawString("time not synced", 160, 68);
  }

  canvas_.setTextDatum(middle_left);
  int y = 88;
  auto row = [&](const char* k, const char* v, uint16_t vc) {
    canvas_.setTextColor(COL_DIM);
    canvas_.drawString(k, 28, y);
    canvas_.setTextColor(vc);
    canvas_.drawString(v, 84, y);
    y += 16;
  };
  if (st.wifi == 0) row("WiFi", "not set - see /chacki/config.txt", COL_AMBER);
  else {
    snprintf(buf, sizeof(buf), "%s%s", st.ssid, st.wifi == 2 ? "" : " ...");
    row("WiFi", buf, st.wifi == 2 ? COL_WHITE : COL_AMBER);
  }
  if (st.wifi == 2) {
    snprintf(buf, sizeof(buf), "http://%s", st.ip);
    row("Web", buf, COL_CYAN);
    snprintf(buf, sizeof(buf), "chacki / %s", st.webPass);
    row("Login", buf, COL_WHITE);
  } else {
    row("Web", "needs WiFi", COL_DIM);
    y += 16;
  }
  row("Brain", !st.backendConfigured ? "not configured" : (st.backendOk ? "online" : "offline"),
      !st.backendConfigured ? COL_DIM : (st.backendOk ? COL_GREEN : COL_RED));
  if (st.battery >= 0) snprintf(buf, sizeof(buf), "%d%%%s   v%s", st.battery, st.charging ? " chg" : "", CHACKI_VERSION);
  else snprintf(buf, sizeof(buf), "v%s", CHACKI_VERSION);
  row("Power", buf, COL_WHITE);

  addButton(28, 190, 124, 36, ButtonId::Launcher, "LAUNCHER", accent_);
  addButton(168, 190, 124, 36, ButtonId::Close, "CLOSE", COL_CYAN);
}

void DisplayManager::drawPetCard(const Scene& sc) {
  panel(16, 30, 288, 200);
  canvas_.setFont(&fonts::Orbitron_Light_24);
  canvas_.setTextSize(0.8f);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(COL_CYAN);
  canvas_.drawString(name_, 160, 50);
  canvas_.setTextSize(1.f);

  auto bar = [&](int y, const char* lbl, float v) {
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_left);
    canvas_.setTextColor(COL_DIM);
    canvas_.drawString(lbl, 32, y);
    uint16_t c = v < 25 ? COL_RED : (v < 50 ? COL_AMBER : COL_GREEN);
    canvas_.drawRoundRect(98, y - 7, 180, 14, 5, COL_DIM);
    canvas_.fillRoundRect(100, y - 5, (int)(176 * v / 100.f), 10, 4, c);
  };
  bar(80, "Food", sc.food);
  bar(104, "Energy", sc.energy);
  bar(128, "Fun", sc.fun);

  canvas_.setFont(&fonts::Font0);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(COL_DIM);
  char buf[40];
  snprintf(buf, sizeof(buf), "focus sessions today: %u", sc.pomoSessions);
  canvas_.drawString(buf, 160, 152);

  addButton(26, 172, 82, 40, ButtonId::Feed, "FEED", COL_AMBER);
  addButton(119, 172, 82, 40, ButtonId::Play, "PLAY", COL_PINK);
  if (sc.pomoActive) addButton(212, 172, 82, 40, ButtonId::StopFocus, "STOP", COL_RED);
  else addButton(212, 172, 82, 40, ButtonId::Focus, "FOCUS", COL_CYAN);
}

void DisplayManager::drawPopup(const Scene& sc, float t) {
  panel(24, 40, 272, 180);
  drawIcon(sc.icon, 160, 76, t);
  canvas_.setTextDatum(middle_center);
  canvas_.setFont(&fonts::Orbitron_Light_24);
  canvas_.setTextSize(0.75f);
  canvas_.setTextColor(COL_CYAN);
  canvas_.drawString(sc.title, 160, 110);
  canvas_.setTextSize(1.f);

  // word-wrap the message into up to 2 lines
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextColor(COL_WHITE);
  String msg = sc.text;
  const int maxW = 248;
  if (canvas_.textWidth(msg) <= maxW) {
    canvas_.drawString(msg, 160, 140);
  } else {
    int cut = msg.length();
    while (cut > 0) {
      cut = msg.lastIndexOf(' ', cut - 1);
      if (cut <= 0) break;
      if (canvas_.textWidth(msg.substring(0, cut)) <= maxW) break;
    }
    if (cut <= 0) cut = msg.length() / 2;
    canvas_.drawString(msg.substring(0, cut), 160, 132);
    canvas_.drawString(msg.substring(cut + 1), 160, 150);
  }

  if (sc.btn2 != ButtonId::None) {
    addButton(40, 172, 112, 36, sc.btn1, label(sc.btn1), COL_CYAN);
    addButton(168, 172, 112, 36, sc.btn2, label(sc.btn2), accent_);
  } else if (sc.btn1 != ButtonId::None) {
    addButton(104, 172, 112, 36, sc.btn1, label(sc.btn1), COL_CYAN);
  }
}

void DisplayManager::drawOta(const Scene& sc) {
  panel(30, 70, 260, 100);
  canvas_.setTextDatum(middle_center);
  canvas_.setFont(&fonts::Orbitron_Light_24);
  canvas_.setTextSize(0.8f);
  canvas_.setTextColor(COL_CYAN);
  canvas_.drawString("UPDATING", 160, 96);
  canvas_.setTextSize(1.f);
  canvas_.drawRect(50, 120, 220, 14, COL_DIM);
  canvas_.fillRect(52, 122, (int)(216 * sc.otaProgress / 100.f), 10, COL_CYAN);
  canvas_.setFont(&fonts::Font0);
  canvas_.setTextColor(COL_DIM);
  canvas_.drawString("don't power off", 160, 150);
}

// ---------------------------------------------------------------- frame -----

void DisplayManager::render(const AnimationEngine& anim, const StatusInfo& st, const Scene& sc) {
  const FaceParams& f = anim.face();
  const uint16_t col = scaled(f.r, f.g, f.b, 1.f);
  nBtns_ = 0;

  canvas_.fillScreen(bg_);
  if (matrix_) drawMatrix(anim.time());
  drawHud();

  float cx = 160.f, cy = 112.f + anim.bob() + anim.bounceOffset();
  float jitter = (anim.mood() == Mood::Error && random(100) < 35) ? (float)random(-6, 7) : 0.f;
  float lx = anim.lookX(), ly = anim.lookY();
  for (int s = -1; s <= 1; s += 2) {
    float sizeK = (1.f + (s < 0 ? f.asym : -f.asym * 0.5f)) * f.scale;
    float ex = cx + s * f.spacing + lx + jitter * s;
    float persp = 1.f + (lx * s) * 0.004f;
    drawEye(ex, cy + ly, f.eyeW * persp * f.scale, f.eyeH * sizeK * persp, s, f, anim.shape(), col, anim.blink());
  }
  drawMouth(anim, cx + lx * 0.4f, cy + 66 + ly * 0.3f, col, st.micLevel);
  drawExtras(anim, cx + lx * 0.3f, cy + ly * 0.3f);
  drawProps(sc, anim.time());
  drawScanlines();
  drawPomoBar(sc);
  drawStatusBar(anim, st, sc);

  if (sc.ota) drawOta(sc);
  else if (sc.card == Scene::Card::Info) drawInfoCard(st);
  else if (sc.card == Scene::Card::Pet) drawPetCard(sc);
  else if (sc.card == Scene::Card::Popup) drawPopup(sc, anim.time());

  canvas_.pushSprite(0, 0);
}
