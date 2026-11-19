#pragma once
#include <stdint.h>

#ifndef CHACKI_VERSION
#define CHACKI_VERSION "dev"
#endif

// ---- Screen -----------------------------------------------------------------
constexpr int SCREEN_W = 320;
constexpr int SCREEN_H = 240;
constexpr int STATUS_BAR_H = 20;
constexpr uint32_t FRAME_MS = 33;  // ~30 fps

// ---- Behaviour timings ------------------------------------------------------
constexpr uint32_t DROWSY_AFTER_MS = 90UL * 1000;   // eyelids start drooping
constexpr uint32_t SLEEP_AFTER_MS = 180UL * 1000;   // falls asleep
constexpr uint32_t INFO_CARD_MS = 10UL * 1000;      // info card auto-close
constexpr uint8_t BRIGHTNESS_AWAKE = 170;
constexpr uint8_t BRIGHTNESS_ASLEEP = 20;

// ---- Audio ------------------------------------------------------------------
constexpr uint32_t MIC_SAMPLE_RATE = 16000;
constexpr uint32_t MAX_RECORD_MS = 6000;
constexpr uint32_t MIN_RECORD_MS = 350;
constexpr uint8_t DEFAULT_VOLUME = 150;

// ---- microSD (CoreS3 / CoreS3-SE, shared SPI bus with the LCD) -------------
constexpr int SD_SCK = 36;
constexpr int SD_MISO = 35;
constexpr int SD_MOSI = 37;
constexpr int SD_CS = 4;
constexpr const char* SD_CONFIG_PATH = "/chacki/config.txt";

// ---- Network ----------------------------------------------------------------
constexpr uint32_t WIFI_RETRY_MS = 15000;
constexpr uint32_t HEALTH_CHECK_MS = 30000;
constexpr uint16_t HTTP_TIMEOUT_MS = 2000;
constexpr const char* DEFAULT_TZ = "IST-5:30";  // POSIX TZ string

// ---- Colours (RGB565) -------------------------------------------------------
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}
constexpr uint16_t COL_BG = rgb565(4, 6, 18);
constexpr uint16_t COL_SCAN = rgb565(1, 2, 8);
constexpr uint16_t COL_CYAN = rgb565(0, 230, 255);
constexpr uint16_t COL_PURPLE = rgb565(170, 70, 255);
constexpr uint16_t COL_PINK = rgb565(255, 60, 170);
constexpr uint16_t COL_RED = rgb565(255, 40, 60);
constexpr uint16_t COL_GREEN = rgb565(40, 255, 140);
constexpr uint16_t COL_AMBER = rgb565(255, 180, 40);
constexpr uint16_t COL_DIM = rgb565(60, 80, 110);
constexpr uint16_t COL_PANEL = rgb565(10, 14, 34);
constexpr uint16_t COL_WHITE = rgb565(230, 245, 255);
