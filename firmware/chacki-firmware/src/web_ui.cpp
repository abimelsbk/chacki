#include "web_ui.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <SD.h>
#include <SPI.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_ota_ops.h>
#include <freertos/queue.h>

#include "config.h"
#include "pet_state.h"
#include "settings.h"
#include "web_page.h"

WebUi gWeb;

namespace {

WebServer server(80);
File upFile;
constexpr const char* AUTH_USER = "chacki";
constexpr const char* SD_UPDATE_PATH = "/chacki/update.bin";
constexpr uint16_t ESP32S3_CHIP_ID = 9;

void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendErr(int code, const String& msg) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  String s;
  serializeJson(d, s);
  sendJson(code, s);
}

}  // namespace

// ------------------------------------------------------------------ setup ----

void WebUi::begin() {
  queue_ = xQueueCreate(8, sizeof(WebCommand));

  const esp_partition_t* run = esp_ota_get_running_partition();
  const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
  otaMode_ = (run && next && next->address != run->address && next->size >= 0x180000) ? OtaMode::Flash
                                                                                      : OtaMode::SdStage;
  log_i("running from %s, OTA mode %s", run ? run->label : "?", otaMode_ == OtaMode::Flash ? "flash" : "sd");

  xTaskCreatePinnedToCore(taskEntry, "chacki-web", 10240, this, 1, nullptr, 0);
}

void WebUi::taskEntry(void* arg) { static_cast<WebUi*>(arg)->taskLoop(); }

void WebUi::publish(const StatusSnapshot& s) {
  portENTER_CRITICAL(&mux_);
  snap_ = s;
  portEXIT_CRITICAL(&mux_);
}

bool WebUi::takeCommand(WebCommand& out) {
  return queue_ && xQueueReceive((QueueHandle_t)queue_, &out, 0) == pdTRUE;
}

void WebUi::push(WebCmd c, uint8_t arg) {
  WebCommand w{c, arg};
  xQueueSend((QueueHandle_t)queue_, &w, 0);
}

bool WebUi::auth() {
  String pass = gSettings.webPass();
  if (server.authenticate(AUTH_USER, pass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "Chacki");
  return false;
}

void WebUi::taskLoop() {
  // wait for the first Wi-Fi connection
  while (WiFi.status() != WL_CONNECTED) vTaskDelay(pdMS_TO_TICKS(500));

  if (otaMode_ == OtaMode::Flash) {
    startArduinoOta();  // also starts mDNS as chacki.local
  } else {
    MDNS.begin("chacki");
  }
  MDNS.addService("http", "tcp", 80);

  setupRoutes();
  server.begin();
  started_ = true;

  for (;;) {
    server.handleClient();
    if (otaMode_ == OtaMode::Flash) ArduinoOTA.handle();
    vTaskDelay(pdMS_TO_TICKS(otaActive_ ? 1 : 4));
  }
}

void WebUi::startArduinoOta() {
  ArduinoOTA.setHostname("chacki");
  ArduinoOTA.setPassword(gSettings.webPass().c_str());
  ArduinoOTA.setRebootOnSuccess(false);
  ArduinoOTA.onStart([this]() {
    otaActive_ = true;
    otaProgress_ = 0;
  });
  ArduinoOTA.onProgress([this](unsigned int done, unsigned int total) {
    otaProgress_ = total ? (int)(done * 100ULL / total) : 0;
  });
  ArduinoOTA.onEnd([this]() {
    otaProgress_ = 100;
    push(WebCmd::UpdatedFlash);
  });
  ArduinoOTA.onError([this](ota_error_t) { otaActive_ = false; });
  ArduinoOTA.begin();
}

// ------------------------------------------------------------------ routes ---

void WebUi::setupRoutes() {
  server.on("/", HTTP_GET, [this]() {
    if (!auth()) return;
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html", CHACKI_PAGE);
  });

  server.on("/api/status", HTTP_GET, [this]() {
    if (!auth()) return;
    StatusSnapshot s;
    portENTER_CRITICAL(&mux_);
    s = snap_;
    portEXIT_CRITICAL(&mux_);
    JsonDocument d;
    d["mood"] = s.mood;
    d["personality"] = s.personality;
    d["food"] = (int)s.food;
    d["energy"] = (int)s.energy;
    d["fun"] = (int)s.fun;
    d["battery"] = s.battery;
    d["charging"] = s.charging;
    d["sleeping"] = s.sleeping;
    d["ssid"] = s.ssid;
    d["ip"] = s.ip;
    d["rssi"] = s.rssi;
    d["time"] = s.time;
    d["uptime"] = s.uptimeS;
    d["pomo"] = s.pomo;
    d["pomo_remain"] = s.pomoRemainS;
    d["sessions"] = s.sessions;
    d["ambient"] = s.ambient;
    d["version"] = CHACKI_VERSION;
    d["device_id"] = gSettings.deviceId();
    d["ota_mode"] = otaMode_ == OtaMode::Flash ? "flash" : "sd";
    d["ota_active"] = (bool)otaActive_;
    d["ota_progress"] = (int)otaProgress_;
    d["free_heap"] = ESP.getFreeHeap();
    String out;
    serializeJson(d, out);
    sendJson(200, out);
  });

  server.on("/api/settings", HTTP_GET, [this]() {
    if (!auth()) return;
    sendJson(200, gSettings.toJson());
  });

  server.on("/api/settings", HTTP_POST, [this]() {
    if (!auth()) return;
    bool reboot = false;
    String err;
    if (!gSettings.applyJson(server.arg("plain"), reboot, err)) return sendErr(400, err);
    sendJson(200, reboot ? "{\"ok\":true,\"reboot\":true}" : "{\"ok\":true,\"reboot\":false}");
  });

  server.on("/api/action", HTTP_POST, [this]() {
    if (!auth()) return;
    JsonDocument d;
    if (deserializeJson(d, server.arg("plain"))) return sendErr(400, "invalid JSON");
    String a = d["action"] | "";
    if (a == "feed") push(WebCmd::Feed);
    else if (a == "play") push(WebCmd::Play);
    else if (a == "pet") push(WebCmd::Pet);
    else if (a == "sleep") push(WebCmd::Sleep);
    else if (a == "wake") push(WebCmd::Wake);
    else if (a == "focus_start") push(WebCmd::FocusStart);
    else if (a == "focus_stop") push(WebCmd::FocusStop);
    else if (a == "test_reminder") push(WebCmd::TestReminder);
    else if (a == "test_alarm") push(WebCmd::TestAlarm);
    else if (a == "test_sound") push(WebCmd::TestSound);
    else if (a == "dismiss") push(WebCmd::Dismiss);
    else if (a == "mood") {
      String m = d["mood"] | "";
      bool found = false;
      for (uint8_t i = 0; i < (uint8_t)Mood::COUNT; i++) {
        if (m.equalsIgnoreCase(moodName((Mood)i))) {
          push(WebCmd::Mood, i);
          found = true;
        }
      }
      if (!found) return sendErr(400, "unknown mood");
    } else return sendErr(400, "unknown action");
    sendJson(200, "{\"ok\":true}");
  });

  server.on("/api/reboot", HTTP_POST, [this]() {
    if (!auth()) return;
    sendJson(200, "{\"ok\":true}");
    push(WebCmd::Reboot);
  });

  server.on("/api/launcher", HTTP_POST, [this]() {
    if (!auth()) return;
    sendJson(200, "{\"ok\":true}");
    push(WebCmd::Launcher);
  });

  server.on("/update", HTTP_POST, [this]() { handleUploadDone(); }, [this]() { handleUpload(); });

  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
}

// ------------------------------------------------------------------ OTA ------

bool WebUi::freezeDisplay() {
  frozen_ = false;
  freezeReq_ = true;
  uint32_t t0 = millis();
  while (!frozen_ && millis() - t0 < 1500) vTaskDelay(pdMS_TO_TICKS(5));
  return frozen_;
}

void WebUi::unfreezeDisplay() {
  freezeReq_ = false;
  frozen_ = false;
}

void WebUi::handleUpload() {
  HTTPUpload& up = server.upload();

  if (up.status == UPLOAD_FILE_START) {
    upOk_ = false;
    upErr_ = "";
    upWritten_ = 0;
    upPrevLast_ = 0;
    upTotal_ = server.clientContentLength();
    upReject_ = !server.authenticate(AUTH_USER, gSettings.webPass().c_str());
    if (upReject_) {
      upErr_ = "unauthorized";
      return;
    }
    if (!up.filename.endsWith(".bin")) {
      upReject_ = true;
      upErr_ = "please upload a .bin file";
      return;
    }
    otaActive_ = true;
    otaProgress_ = 0;
    if (otaMode_ == OtaMode::Flash) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) {
        upReject_ = true;
        upErr_ = String("update begin failed: ") + Update.errorString();
      }
    } else {
      if (!freezeDisplay()) {
        upReject_ = true;
        upErr_ = "display busy, try again";
        return;
      }
      SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
      if (!SD.begin(SD_CS, SPI, 20000000)) {
        upReject_ = true;
        upErr_ = "no SD card - insert the SD card used by M5 Launcher";
        unfreezeDisplay();
        return;
      }
      SD.mkdir("/chacki");
      SD.remove(SD_UPDATE_PATH);
      upFile = SD.open(SD_UPDATE_PATH, FILE_WRITE);
      if (!upFile) {
        upReject_ = true;
        upErr_ = "could not create file on SD";
        SD.end();
        unfreezeDisplay();
      }
    }
    return;
  }

  if (up.status == UPLOAD_FILE_WRITE) {
    if (upReject_) return;
    const uint8_t* b = up.buf;
    size_t n = up.currentSize;

    // first chunk: must be an ESP32-S3 image
    if (upWritten_ == 0) {
      if (n < 16 || b[0] != 0xE9 || (uint16_t)(b[12] | (b[13] << 8)) != ESP32S3_CHIP_ID) {
        upReject_ = true;
        upErr_ = "not an ESP32-S3 firmware image";
        if (otaMode_ == OtaMode::Flash) Update.abort();
        return;
      }
    }
    // flash mode: refuse merged images (partition table magic AA 50 at 0x8000)
    if (otaMode_ == OtaMode::Flash && upWritten_ <= 0x8000 && upWritten_ + n > 0x8000) {
      size_t off = 0x8000 - upWritten_;
      uint8_t b0 = b[off];
      uint8_t b1 = off + 1 < n ? b[off + 1] : 0;
      bool merged = b0 == 0xAA && (off + 1 < n ? b1 == 0x50 : true);
      if (merged) {
        upReject_ = true;
        upErr_ = "this is the -full.bin; upload Chacki-CoreS3SE-launcher.bin instead";
        Update.abort();
        return;
      }
    }

    size_t w = otaMode_ == OtaMode::Flash ? Update.write((uint8_t*)b, n) : upFile.write(b, n);
    if (w != n) {
      upReject_ = true;
      upErr_ = otaMode_ == OtaMode::Flash ? String("flash write failed: ") + Update.errorString()
                                          : String("SD write failed (card full?)");
      if (otaMode_ == OtaMode::Flash) Update.abort();
      return;
    }
    upWritten_ += n;
    if (upTotal_) otaProgress_ = (int)(upWritten_ * 100ULL / upTotal_);
    return;
  }

  if (up.status == UPLOAD_FILE_END) {
    if (!upReject_) {
      if (otaMode_ == OtaMode::Flash) {
        upOk_ = Update.end(true);
        if (!upOk_) upErr_ = String("verify failed: ") + Update.errorString();
      } else {
        upOk_ = upWritten_ > 64 * 1024;
        if (!upOk_) upErr_ = "file too small";
      }
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    upReject_ = true;
    upErr_ = "upload aborted";
    if (otaMode_ == OtaMode::Flash) Update.abort();
  }

  // END or ABORTED: clean up SD
  if (otaMode_ == OtaMode::SdStage && upFile) {
    upFile.close();
    if (!upOk_) SD.remove(SD_UPDATE_PATH);
    SD.end();
  }
  if (otaMode_ == OtaMode::SdStage) unfreezeDisplay();
}

void WebUi::handleUploadDone() {
  if (upErr_ == "unauthorized") {
    otaActive_ = false;
    server.requestAuthentication(BASIC_AUTH, "Chacki");
    return;
  }
  if (!upOk_) {
    otaActive_ = false;
    if (otaMode_ == OtaMode::SdStage) unfreezeDisplay();
    return sendErr(400, upErr_.length() ? upErr_ : "update failed");
  }
  otaProgress_ = 100;
  if (otaMode_ == OtaMode::Flash) {
    sendJson(200, "{\"ok\":true,\"mode\":\"flash\",\"message\":\"Updated - rebooting\"}");
    push(WebCmd::UpdatedFlash);
  } else {
    sendJson(200,
             "{\"ok\":true,\"mode\":\"sd\",\"message\":\"Saved to SD as /chacki/update.bin - Chacki is rebooting "
             "into Launcher. Install it from SD > chacki > update.bin\"}");
    push(WebCmd::UpdatedSd);
  }
}
