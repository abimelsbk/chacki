#include "network_manager.h"

#include <HTTPClient.h>
#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <freertos/queue.h>

#include "config.h"

namespace {

struct EventMsg {
  char type[16];
  char mood[12];
};

}  // namespace

// ------------------------------------------------------------- clock -------

bool NetworkManager::timeValid() const { return time(nullptr) > 1700000000; }

void NetworkManager::restoreClockFromRtc() {
  if (!M5.Rtc.isEnabled()) return;
  auto dt = M5.Rtc.getDateTime();
  if (dt.date.year < 2024) return;
  struct tm t {};
  t.tm_year = dt.date.year - 1900;
  t.tm_mon = dt.date.month - 1;
  t.tm_mday = dt.date.date;
  t.tm_hour = dt.time.hours;
  t.tm_min = dt.time.minutes;
  t.tm_sec = dt.time.seconds;
  t.tm_isdst = -1;
  time_t epoch = mktime(&t);  // RTC holds local time
  struct timeval tv = {epoch, 0};
  settimeofday(&tv, nullptr);
}

// ------------------------------------------------------------- task --------

void NetworkManager::begin(const Settings& b) {
  for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
    wifiSsid_[i] = b.wifiSsid[i];
    wifiPass_[i] = b.wifiPass[i];
  }
  backendUrl_ = b.backendUrl;
  token_ = b.deviceToken;
  tz_ = b.tz;
  if (b.wifiCount() == 0) {
    wifiState_ = 0;
    return;
  }
  queue_ = xQueueCreate(8, sizeof(EventMsg));
  xTaskCreatePinnedToCore(taskEntry, "chacki-net", 8192, this, 1, nullptr, 0);
}

void NetworkManager::taskEntry(void* arg) { static_cast<NetworkManager*>(arg)->taskLoop(); }

void NetworkManager::sendPetEvent(const char* type, const char* mood) {
  if (!queue_ || !backendConfigured() || wifiState_ != 2) return;
  EventMsg m{};
  strlcpy(m.type, type, sizeof(m.type));
  strlcpy(m.mood, mood, sizeof(m.mood));
  xQueueSend((QueueHandle_t)queue_, &m, 0);
}

void NetworkManager::checkHealth() {
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  if (!http.begin(backendUrl_ + "/api/v1/health")) {
    backendOk_ = false;
    return;
  }
  if (token_.length()) http.addHeader("Authorization", "Bearer " + token_);
  backendOk_ = http.GET() == 200;
  http.end();
}

void NetworkManager::flushEvents() {
  EventMsg m;
  while (xQueueReceive((QueueHandle_t)queue_, &m, 0) == pdTRUE) {
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    if (!http.begin(backendUrl_ + "/api/v1/pet/event")) continue;
    http.addHeader("Content-Type", "application/json");
    if (token_.length()) http.addHeader("Authorization", "Bearer " + token_);
    char body[96];
    snprintf(body, sizeof(body), "{\"device_id\":\"%s\",\"type\":\"%s\",\"mood\":\"%s\"}", gSettings.deviceId(), m.type,
             m.mood);
    int code = http.POST((uint8_t*)body, strlen(body));
    http.end();
    if (code <= 0) backendOk_ = false;
  }
}

void NetworkManager::taskLoop() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(gSettings.deviceId());
  WiFiMulti multi;  // scans and joins the strongest known network in range
  for (uint8_t i = 0; i < MAX_WIFI_NETWORKS; i++) {
    if (wifiSsid_[i].length()) multi.addAP(wifiSsid_[i].c_str(), wifiPass_[i].c_str());
  }
  wifiState_ = 1;
  strlcpy(ssid_, "searching...", sizeof(ssid_));

  uint32_t lastAttempt = 0;
  uint32_t lastHealth = 0;
  bool ntpStarted = false;

  for (;;) {
    uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      wifiState_ = 1;
      backendOk_ = false;
      if (lastAttempt == 0 || now - lastAttempt > WIFI_RETRY_MS) {
        multi.run(8000);
        lastAttempt = millis();
      }
    } else {
      wifiState_ = 2;
      rssi_ = WiFi.RSSI();
      strlcpy(ip_, WiFi.localIP().toString().c_str(), sizeof(ip_));
      strlcpy(ssid_, WiFi.SSID().c_str(), sizeof(ssid_));

      if (!ntpStarted) {
        configTzTime(tz_.c_str(), "pool.ntp.org", "time.google.com");
        ntpStarted = true;
      }
      if (!rtcWritten_ && timeValid()) {
        time_t t = time(nullptr);
        struct tm lt;
        localtime_r(&t, &lt);
        if (M5.Rtc.isEnabled()) M5.Rtc.setDateTime(&lt);
        rtcWritten_ = true;
      }
      if (backendConfigured()) {
        if (lastHealth == 0 || now - lastHealth > HEALTH_CHECK_MS) {
          checkHealth();
          lastHealth = now;
        }
        flushEvents();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}
