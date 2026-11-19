#include "settings.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>

#include "config.h"
#include "secrets.h"
#include "skins.h"

SettingsStore gSettings;

namespace {

const char* PERSONALITIES[] = {"cheerful", "sarcastic", "sleepy", "hacker"};
const char* SHAPES[] = {"round", "square", "pill"};

uint32_t fnv1a(const String& s) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < s.length(); i++) {
    h ^= (uint8_t)s[i];
    h *= 16777619u;
  }
  return h;
}

struct Guard {
  SemaphoreHandle_t m;
  explicit Guard(SemaphoreHandle_t mm) : m(mm) { xSemaphoreTake(m, portMAX_DELAY); }
  ~Guard() { xSemaphoreGive(m); }
};

String genPassword() {
  const char* cs = "abcdefghjkmnpqrstuvwxyz23456789";
  String p;
  for (int i = 0; i < 8; i++) p += cs[esp_random() % 31];
  return p;
}

bool parseHHMM(const String& v, uint8_t& h, uint8_t& m) {
  int c = v.indexOf(':');
  if (c <= 0) return false;
  int hh = v.substring(0, c).toInt(), mm = v.substring(c + 1).toInt();
  if (hh < 0 || hh > 23 || mm < 0 || mm > 59) return false;
  h = hh;
  m = mm;
  return true;
}

}  // namespace

const char* personalityName(Personality p) {
  return (uint8_t)p < (uint8_t)Personality::COUNT ? PERSONALITIES[(uint8_t)p] : "cheerful";
}

bool personalityFromName(const String& s, Personality& out) {
  for (uint8_t i = 0; i < (uint8_t)Personality::COUNT; i++)
    if (s.equalsIgnoreCase(PERSONALITIES[i])) {
      out = (Personality)i;
      return true;
    }
  return false;
}

const char* eyeShapeName(EyeShape e) { return (uint8_t)e < (uint8_t)EyeShape::COUNT ? SHAPES[(uint8_t)e] : "round"; }

uint16_t rgb888to565(uint32_t c) {
  uint8_t r = c >> 16, g = c >> 8, b = c;
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

uint32_t parseHexColor(const String& s, uint32_t fallback) {
  String v = s;
  v.trim();
  if (v.startsWith("#")) v.remove(0, 1);
  if (v.length() != 6) return fallback;
  for (size_t i = 0; i < 6; i++)
    if (!isxdigit((unsigned char)v[i])) return fallback;
  return (uint32_t)strtoul(v.c_str(), nullptr, 16);
}

String hexColor(uint32_t c) {
  char b[8];
  snprintf(b, sizeof(b), "#%06x", (unsigned)(c & 0xFFFFFF));
  return b;
}

// ------------------------------------------------------------------ NVS ------

void SettingsStore::loadNvs(Settings& s, bool& found) {
  Preferences p;
  found = false;
  if (!p.begin("chacki", true)) return;
  found = p.isKey("ssid0");
  char k[8];
  for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
    snprintf(k, sizeof(k), "ssid%u", i);
    s.wifiSsid[i] = p.getString(k, s.wifiSsid[i]);
    snprintf(k, sizeof(k), "pass%u", i);
    s.wifiPass[i] = p.getString(k, s.wifiPass[i]);
  }
  s.backendUrl = p.getString("backend", s.backendUrl);
  s.deviceToken = p.getString("token", s.deviceToken);
  s.tz = p.getString("tz", s.tz);
  s.webPass = p.getString("webpass", s.webPass);
  s.name = p.getString("name", s.name);
  s.personality = (Personality)p.getUChar("persona", (uint8_t)s.personality);
  if ((uint8_t)s.personality >= (uint8_t)Personality::COUNT) s.personality = Personality::Cheerful;
  s.skinName = p.getString("skin", s.skinName);
  s.eyeColor = p.getUInt("eye", s.eyeColor);
  s.accent = p.getUInt("accent", s.accent);
  s.bgColor = p.getUInt("bg", s.bgColor);
  s.eyeShape = (EyeShape)p.getUChar("shape", (uint8_t)s.eyeShape);
  if ((uint8_t)s.eyeShape >= (uint8_t)EyeShape::COUNT) s.eyeShape = EyeShape::Round;
  s.scanlines = p.getBool("scan", s.scanlines);
  s.brightness = p.getUChar("bright", s.brightness);
  s.volume = p.getUChar("volume", s.volume);
  s.sfx = p.getBool("sfx", s.sfx);
  s.clapOn = p.getBool("clap", s.clapOn);
  s.clapSens = p.getUChar("clapsens", s.clapSens);
  s.nightOn = p.getBool("night", s.nightOn);
  s.nightStart = p.getUChar("nstart", s.nightStart);
  s.nightEnd = p.getUChar("nend", s.nightEnd);
  s.alarmOn = p.getBool("alarm", s.alarmOn);
  s.alarmHour = p.getUChar("alh", s.alarmHour);
  s.alarmMin = p.getUChar("alm", s.alarmMin);
  s.waterMin = p.getUShort("water", s.waterMin);
  s.standMin = p.getUShort("stand", s.standMin);
  s.customMin = p.getUShort("cmin", s.customMin);
  s.customText = p.getString("ctext", s.customText);
  s.pomoFocus = p.getUChar("pfocus", s.pomoFocus);
  s.pomoBreak = p.getUChar("pbreak", s.pomoBreak);
  s.pomoLong = p.getUChar("plong", s.pomoLong);
  p.end();
}

void SettingsStore::saveNvs(const Settings& s) {
  Preferences p;
  if (!p.begin("chacki", false)) return;
  char k[8];
  for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
    snprintf(k, sizeof(k), "ssid%u", i);
    p.putString(k, s.wifiSsid[i]);
    snprintf(k, sizeof(k), "pass%u", i);
    p.putString(k, s.wifiPass[i]);
  }
  p.putString("backend", s.backendUrl);
  p.putString("token", s.deviceToken);
  p.putString("tz", s.tz);
  p.putString("webpass", s.webPass);
  p.putString("name", s.name);
  p.putUChar("persona", (uint8_t)s.personality);
  p.putString("skin", s.skinName);
  p.putUInt("eye", s.eyeColor);
  p.putUInt("accent", s.accent);
  p.putUInt("bg", s.bgColor);
  p.putUChar("shape", (uint8_t)s.eyeShape);
  p.putBool("scan", s.scanlines);
  p.putUChar("bright", s.brightness);
  p.putUChar("volume", s.volume);
  p.putBool("sfx", s.sfx);
  p.putBool("clap", s.clapOn);
  p.putUChar("clapsens", s.clapSens);
  p.putBool("night", s.nightOn);
  p.putUChar("nstart", s.nightStart);
  p.putUChar("nend", s.nightEnd);
  p.putBool("alarm", s.alarmOn);
  p.putUChar("alh", s.alarmHour);
  p.putUChar("alm", s.alarmMin);
  p.putUShort("water", s.waterMin);
  p.putUShort("stand", s.standMin);
  p.putUShort("cmin", s.customMin);
  p.putString("ctext", s.customText);
  p.putUChar("pfocus", s.pomoFocus);
  p.putUChar("pbreak", s.pomoBreak);
  p.putUChar("plong", s.pomoLong);
  p.end();
}

// ------------------------------------------------------------------ SD -------

// Imports /chacki/config.txt when its content differs from the last import.
bool SettingsStore::importSd(Settings& s) {
  File f = SD.open(SD_CONFIG_PATH, FILE_READ);
  if (!f) return false;
  String content;
  while (f.available() && content.length() < 8192) content += (char)f.read();
  f.close();

  uint32_t h = fnv1a(content);
  Preferences p;
  p.begin("chacki", true);
  uint32_t last = p.getUInt("sdhash", 0);
  p.end();
  if (h == last) return false;

  bool sawWifi = false;
  int start = 0;
  while (start < (int)content.length()) {
    int nl = content.indexOf('\n', start);
    if (nl < 0) nl = content.length();
    String line = content.substring(start, nl);
    start = nl + 1;
    line.trim();
    if (!line.length() || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq <= 0) continue;
    String key = line.substring(0, eq), val = line.substring(eq + 1);
    key.trim();
    val.trim();
    key.toLowerCase();

    if (key.startsWith("wifi") && (key.endsWith("_ssid") || key.endsWith("_pass"))) {
      int idx = 0;
      char c = key.length() > 4 ? key[4] : '_';
      if (c >= '2' && c <= '0' + MAX_WIFI_NETWORKS) idx = c - '1';
      else if (c != '_') continue;
      if (!sawWifi) {
        for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) s.wifiSsid[i] = s.wifiPass[i] = "";
        sawWifi = true;
      }
      if (key.endsWith("_ssid")) s.wifiSsid[idx] = val;
      else s.wifiPass[idx] = val;
    } else if (key == "backend_url") {
      while (val.endsWith("/")) val.remove(val.length() - 1);
      s.backendUrl = val;
    } else if (key == "device_token") s.deviceToken = val;
    else if (key == "tz") s.tz = val;
    else if (key == "volume") s.volume = constrain(val.toInt(), 0, 255);
    else if (key == "web_pass" && val.length() >= 6) s.webPass = val;
    else if (key == "name" && val.length()) s.name = val.substring(0, 12);
    else if (key == "personality") personalityFromName(val, s.personality);
    else if (key == "skin") {
      if (const Skin* sk = gSkins.find(val)) gSkins.applyTo(*sk, s);
    } else if (key == "alarm") {
      s.alarmOn = parseHHMM(val, s.alarmHour, s.alarmMin);
    } else if (key == "water_min") s.waterMin = val.toInt();
    else if (key == "stand_min") s.standMin = val.toInt();
  }

  p.begin("chacki", false);
  p.putUInt("sdhash", h);
  p.end();
  return true;
}

// ------------------------------------------------------------------ public ---

SettingsStore::Source SettingsStore::begin() {
  mtx_ = xSemaphoreCreateMutex();
  uint64_t mac = ESP.getEfuseMac();
  snprintf(deviceId_, sizeof(deviceId_), "chacki-%02x%02x%02x", (uint8_t)(mac >> 24), (uint8_t)(mac >> 32),
           (uint8_t)(mac >> 40));

  gSkins.begin();
  Settings s;
  s.wifiSsid[0] = CHACKI_WIFI_SSID;
  s.wifiPass[0] = CHACKI_WIFI_PASS;
  s.backendUrl = CHACKI_BACKEND_URL;
  s.deviceToken = CHACKI_DEVICE_TOKEN;

  bool found = false;
  loadNvs(s, found);
  Source src = found ? Source::Nvs : Source::Defaults;

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, SPI, 20000000)) {
    gSkins.loadFromSd();
    if (importSd(s)) src = Source::SdCard;
    SD.end();
  }

  bool save = src == Source::SdCard;
  if (s.webPass.length() < 6) {
    s.webPass = genPassword();
    save = true;
  }
  if (save) saveNvs(s);

  stored_ = s;
  live_ = s;
  boot_ = s;
  setenv("TZ", s.tz.c_str(), 1);
  tzset();
  return src;
}

void SettingsStore::copyLiveFields(Settings& d, const Settings& s) {
  d.name = s.name;
  d.personality = s.personality;
  d.skinName = s.skinName;
  d.eyeColor = s.eyeColor;
  d.accent = s.accent;
  d.bgColor = s.bgColor;
  d.eyeShape = s.eyeShape;
  d.scanlines = s.scanlines;
  d.brightness = s.brightness;
  d.volume = s.volume;
  d.sfx = s.sfx;
  d.clapOn = s.clapOn;
  d.clapSens = s.clapSens;
  d.nightOn = s.nightOn;
  d.nightStart = s.nightStart;
  d.nightEnd = s.nightEnd;
  d.alarmOn = s.alarmOn;
  d.alarmHour = s.alarmHour;
  d.alarmMin = s.alarmMin;
  d.waterMin = s.waterMin;
  d.standMin = s.standMin;
  d.customMin = s.customMin;
  d.customText = s.customText;
  d.pomoFocus = s.pomoFocus;
  d.pomoBreak = s.pomoBreak;
  d.pomoLong = s.pomoLong;
}

bool SettingsStore::syncLive() {
  if (!dirty_) return false;
  Guard g(mtx_);
  copyLiveFields(live_, stored_);
  dirty_ = false;
  return true;
}

String SettingsStore::webPass() {
  Guard g(mtx_);
  return stored_.webPass;
}

String SettingsStore::toJson() {
  JsonDocument d;
  {
    Guard g(mtx_);
    const Settings& s = stored_;
    d["name"] = s.name;
    d["personality"] = personalityName(s.personality);
    d["skin"] = s.skinName;
    d["eye_color"] = hexColor(s.eyeColor);
    d["accent"] = hexColor(s.accent);
    d["bg"] = hexColor(s.bgColor);
    d["eye_shape"] = eyeShapeName(s.eyeShape);
    d["scanlines"] = s.scanlines;
    d["brightness"] = s.brightness;
    d["volume"] = s.volume;
    d["sfx"] = s.sfx;
    d["clap_on"] = s.clapOn;
    d["clap_sens"] = s.clapSens;
    d["night_on"] = s.nightOn;
    d["night_start"] = s.nightStart;
    d["night_end"] = s.nightEnd;
    d["alarm_on"] = s.alarmOn;
    char al[6];
    snprintf(al, sizeof(al), "%02u:%02u", s.alarmHour, s.alarmMin);
    d["alarm_time"] = al;
    d["water_min"] = s.waterMin;
    d["stand_min"] = s.standMin;
    d["custom_min"] = s.customMin;
    d["custom_text"] = s.customText;
    d["pomo_focus"] = s.pomoFocus;
    d["pomo_break"] = s.pomoBreak;
    d["pomo_long"] = s.pomoLong;
    d["tz"] = s.tz;
    d["backend_url"] = s.backendUrl;
    d["has_token"] = s.deviceToken.length() > 0;
    JsonArray w = d["wifi"].to<JsonArray>();
    for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
      JsonObject o = w.add<JsonObject>();
      o["ssid"] = s.wifiSsid[i];
      o["has_pass"] = s.wifiPass[i].length() > 0;
    }
  }
  JsonArray sk = d["skins"].to<JsonArray>();
  for (size_t i = 0; i < gSkins.count(); i++) sk.add(gSkins.at(i).name);
  JsonArray pe = d["personalities"].to<JsonArray>();
  for (auto n : PERSONALITIES) pe.add(n);
  String out;
  serializeJson(d, out);
  return out;
}

bool SettingsStore::applyJson(const String& body, bool& needsReboot, String& err) {
  JsonDocument d;
  if (deserializeJson(d, body)) {
    err = "invalid JSON";
    return false;
  }
  needsReboot = false;
  Guard g(mtx_);
  Settings s = stored_;

  auto u8 = [&](const char* k, uint8_t& v, int lo, int hi) {
    if (d[k].is<int>()) v = constrain(d[k].as<int>(), lo, hi);
  };
  auto u16 = [&](const char* k, uint16_t& v, int lo, int hi) {
    if (d[k].is<int>()) v = constrain(d[k].as<int>(), lo, hi);
  };
  auto bl = [&](const char* k, bool& v) {
    if (d[k].is<bool>()) v = d[k].as<bool>();
  };

  if (d["skin"].is<const char*>()) {
    const Skin* sk = gSkins.find(d["skin"].as<String>());
    if (!sk) {
      err = "unknown skin";
      return false;
    }
    gSkins.applyTo(*sk, s);
  }
  if (d["name"].is<const char*>()) {
    String n = d["name"].as<String>();
    n.trim();
    if (n.length()) s.name = n.substring(0, 12);
  }
  if (d["personality"].is<const char*>() && !personalityFromName(d["personality"].as<String>(), s.personality)) {
    err = "unknown personality";
    return false;
  }
  if (d["eye_color"].is<const char*>()) s.eyeColor = parseHexColor(d["eye_color"].as<String>(), s.eyeColor);
  if (d["accent"].is<const char*>()) s.accent = parseHexColor(d["accent"].as<String>(), s.accent);
  if (d["bg"].is<const char*>()) s.bgColor = parseHexColor(d["bg"].as<String>(), s.bgColor);
  if (d["eye_shape"].is<const char*>()) {
    for (uint8_t i = 0; i < (uint8_t)EyeShape::COUNT; i++)
      if (d["eye_shape"].as<String>().equalsIgnoreCase(SHAPES[i])) s.eyeShape = (EyeShape)i;
  }
  bl("scanlines", s.scanlines);
  u8("brightness", s.brightness, 10, 255);
  u8("volume", s.volume, 0, 255);
  bl("sfx", s.sfx);
  bl("clap_on", s.clapOn);
  u8("clap_sens", s.clapSens, 1, 10);
  bl("night_on", s.nightOn);
  u8("night_start", s.nightStart, 0, 23);
  u8("night_end", s.nightEnd, 0, 23);
  bl("alarm_on", s.alarmOn);
  if (d["alarm_time"].is<const char*>() && !parseHHMM(d["alarm_time"].as<String>(), s.alarmHour, s.alarmMin)) {
    err = "alarm_time must be HH:MM";
    return false;
  }
  u16("water_min", s.waterMin, 0, 600);
  u16("stand_min", s.standMin, 0, 600);
  u16("custom_min", s.customMin, 0, 1440);
  if (d["custom_text"].is<const char*>()) s.customText = d["custom_text"].as<String>().substring(0, 40);
  u8("pomo_focus", s.pomoFocus, 1, 120);
  u8("pomo_break", s.pomoBreak, 1, 60);
  u8("pomo_long", s.pomoLong, 1, 60);

  // network settings -> reboot
  if (d["tz"].is<const char*>() && d["tz"].as<String>() != s.tz) {
    s.tz = d["tz"].as<String>().substring(0, 40);
    needsReboot = true;
  }
  if (d["backend_url"].is<const char*>()) {
    String v = d["backend_url"].as<String>();
    v.trim();
    while (v.endsWith("/")) v.remove(v.length() - 1);
    if (v.length() && !v.startsWith("http://") && !v.startsWith("https://")) {
      err = "backend_url must start with http:// or https://";
      return false;
    }
    if (v != s.backendUrl) needsReboot = true;
    s.backendUrl = v;
  }
  if (d["device_token"].is<const char*>()) {
    s.deviceToken = d["device_token"].as<String>();
    needsReboot = true;
  }
  if (d["web_pass"].is<const char*>()) {
    String v = d["web_pass"].as<String>();
    if (v.length() < 6) {
      err = "web password must be at least 6 characters";
      return false;
    }
    s.webPass = v;
    needsReboot = true;
  }
  if (d["wifi"].is<JsonArray>()) {
    JsonArray w = d["wifi"].as<JsonArray>();
    for (uint8_t i = 0; i < MAX_WIFI_NETWORKS && i < w.size(); i++) {
      JsonObject o = w[i];
      if (o["ssid"].is<const char*>()) {
        String ssid = o["ssid"].as<String>();
        ssid.trim();
        if (ssid != s.wifiSsid[i]) {
          s.wifiSsid[i] = ssid;
          if (!o["pass"].is<const char*>()) s.wifiPass[i] = "";  // new network, no password given
        }
      }
      if (o["pass"].is<const char*>()) s.wifiPass[i] = o["pass"].as<String>();
      if (!s.wifiSsid[i].length()) s.wifiPass[i] = "";
    }
    needsReboot = true;
  }

  stored_ = s;
  saveNvs(stored_);
  dirty_ = true;
  return true;
}
