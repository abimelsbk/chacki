#pragma once
#include <Arduino.h>
#include <time.h>

#include "settings.h"

// Runs all blocking network work (Wi-Fi roaming between saved networks, NTP,
// backend health checks, event uploads) on a FreeRTOS task pinned to core 0,
// so the face animation on core 1 never stutters.
class NetworkManager {
 public:
  void begin(const Settings& boot);  // starts the background task
  void restoreClockFromRtc();

  uint8_t wifiState() const { return wifiState_; }  // 0 off, 1 connecting, 2 connected
  int rssi() const { return rssi_; }
  const char* ip() const { return ip_; }
  const char* ssid() const { return ssid_; }
  bool backendConfigured() const { return backendUrl_.length() > 0; }
  bool backendOk() const { return backendOk_; }
  bool timeValid() const;

  void sendPetEvent(const char* type, const char* mood);

 private:
  static void taskEntry(void* arg);
  void taskLoop();
  void checkHealth();
  void flushEvents();

  String wifiSsid_[MAX_WIFI_NETWORKS], wifiPass_[MAX_WIFI_NETWORKS];
  String backendUrl_, token_, tz_;
  char ip_[16] = "";
  char ssid_[33] = "";
  volatile uint8_t wifiState_ = 0;
  volatile int rssi_ = 0;
  volatile bool backendOk_ = false;
  volatile bool rtcWritten_ = false;
  void* queue_ = nullptr;
};
