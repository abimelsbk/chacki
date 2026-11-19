#pragma once
// Optional compiled-in defaults. Copy to secrets.h (git-ignored).
// You normally don't need this: put a /chacki/config.txt on the SD card instead
// (handy with M5 Launcher, which already uses the SD card).
//
// Never put LLM / STT / TTS provider API keys here - those live on the backend.

#define CHACKI_WIFI_SSID ""
#define CHACKI_WIFI_PASS ""
#define CHACKI_BACKEND_URL ""   // e.g. "http://192.168.1.50:8000"
#define CHACKI_DEVICE_TOKEN ""
