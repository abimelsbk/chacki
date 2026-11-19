# Chacki firmware v0.2

Cyberpunk desktop pet for the **M5Stack CoreS3-SE**, built to run as an app under **M5 Launcher**.

## Features

| | |
|---|---|
| **Face** | Procedural neon face, 12 moods (idle, happy, sad, angry, curious, thinking, listening, talking, sleeping, error, excited, focused) with smooth morphing, blinking, glancing, breathing |
| **Pet needs** | Food / energy / fun decay over time (saved across reboots). Feed it data cubes, play ball, pet it. Low needs → thought bubbles and a sad face |
| **Personalities** | Cheerful, sarcastic (eye rolls), sleepy (yawns, naps sooner), hacker (matrix rain, scans). Each has its own lines, sound pitch and quirks |
| **Skins** | 7 built-in (Neon Cyan, Synthwave, Matrix, Amber Term, Ice, Blood Moon, Toxic) plus your own from the SD card. Eye/accent/background colour, eye shape, scanlines |
| **Ears** | Clap = startled, double clap = wake / excited, talking nearby = curious. Adjustable sensitivity |
| **Focus buddy** | Pomodoro 25/5 (configurable, long break every 4th). Focused face, countdown in the status bar, progress line, celebration at the end |
| **Reminders** | Drink water, stand up, one custom reminder, daily alarm with snooze |
| **Night mode** | Says good night and sleeps at night (default 23:00–07:00), wakes you with good morning |
| **Push-to-talk** | Hold the screen, speak, release: robot-voice echo (stand-in until the AI backend exists) |
| **Web UI** | Control everything from your phone |
| **Wireless updates** | Upload new firmware from the browser (see below) |
| **Wi-Fi** | Up to 5 saved networks, joins the strongest; NTP time kept in the RTC |

## Controls

| On the device | |
|---|---|
| Tap face / eye / double tap / 6 fast taps | happy / ouch / curious / angry |
| Rub back and forth | petting (purr) |
| Hold finger still | push-to-talk |
| Swipe left or right | **Pet card**: needs bars, FEED, PLAY, FOCUS/STOP |
| Swipe up | **Info card**: time, Wi-Fi, **web address + login**, LAUNCHER |
| Swipe down | sleep |
| Power button | sleep / wake (or dismiss a pop-up) |

## Web UI

1. Swipe up on Chacki. The info card shows `http://<ip>` and the login (`chacki` / password).
2. Open that address on a phone or PC **on the same Wi-Fi**. `http://chacki.local` also works on most PCs.

From the web UI you can:

- Feed, play, pet, put to sleep or wake Chacki, and trigger any mood.
- Start or stop focus sessions and set their durations.
- Change personality, name, skin, colours, eye shape, scanlines and brightness.
- Set volume and sound effects, toggle clap reaction, and adjust its sensitivity with a live room-level meter.
- Configure night mode, the alarm, and the water/stand/custom reminders, with test buttons.
- Manage Wi-Fi networks (passwords are never sent back to the browser), the backend URL/token and the timezone.
- Change the web password.
- Upload firmware, reboot, or exit to Launcher.

Settings save instantly to the device's flash. The web password is generated on first boot. You can also set it with `web_pass=` in `config.txt`.

> The page uses plain HTTP with Basic auth, so anyone who can sniff your Wi-Fi can read the password. That's fine at home. On shared or cafe networks, change it afterwards, or don't open the UI there.

## Wireless firmware updates

Chacki checks at boot whether it can safely update itself. The **Firmware** section of the web UI shows which mode it's in:

| Mode | When | How it works |
|---|---|---|
| **Via SD + Launcher** | Running under M5 Launcher (one app slot) | Upload `Chacki-CoreS3SE-launcher.bin` in the browser. It's written to the SD card as `/chacki/update.bin`, and Chacki reboots into Launcher. Then tap **SD → chacki → update.bin → Install**. You don't have to take the card out. |
| **Direct** | Flashed directly with `Chacki-CoreS3SE-full.bin` (two app slots) | Upload `Chacki-CoreS3SE-launcher.bin` in the browser, and Chacki flashes itself and reboots. Or straight from VS Code: `set CHACKI_PASS=<web password>` then `pio run -e cores3se-ota -t upload`. |

Why two modes: an app can only rewrite itself if there's a spare slot to write into. Under Launcher, writing over the running slot would brick Chacki, so it never does that.

Safety checks on every upload:

- Requires the web password.
- The file must be an ESP32-S3 image.
- In direct mode, merged `-full.bin` images are refused.
- Images aren't cryptographically signed, so only upload builds you made.

## SD card layout

Start from `sdcard/chacki/config.example.txt`. Rename it to `config.txt` and fill in your Wi-Fi. `config.txt` is git-ignored, so your passwords never end up in the repo.

```
/Chacki.bin                 ← dist\Chacki-CoreS3SE-launcher.bin (install with Launcher)
/chacki/config.txt          ← Wi-Fi etc. (re-imported only when you edit it)
/chacki/skins/*.txt         ← optional custom skins
/chacki/update.bin          ← written by web updates in Launcher mode
```

Custom skin file (`/chacki/skins/Vaporwave.txt`):

```
name=Vaporwave
eye=#ff71ce
accent=#01cdfe
bg=#1a0033
shape=pill        # round | square | pill
scanlines=1
```

Skins are loaded at boot. Pick them in the web UI under **Skin**.

## Build

```powershell
pio run
```

The build writes `dist/Chacki-CoreS3SE-launcher.bin` (for Launcher and web updates) and `dist/Chacki-CoreS3SE-full.bin` (for flashing at 0x0; replaces Launcher).

## Code map

| File | Role |
|---|---|
| `main.cpp` | Orchestration: input, cards, pop-ups, activities, schedule, web commands |
| `settings.cpp` | All settings: NVS, SD import (only when changed), JSON for the web UI |
| `skins.cpp` / `personality.cpp` | Built-in + SD skins / personality traits and phrases |
| `pet_state.cpp` / `pet_needs.cpp` | Mood state machine / food-energy-fun |
| `scheduler.cpp` | Night mode, reminders, alarm, Pomodoro |
| `animation_engine.cpp` | Per-mood face targets, gestures (eye roll, yawn, scan, startle, chomp...) |
| `display_manager.cpp` | Rendering: face, props, status bar, cards, pop-ups, themes, matrix rain |
| `audio_manager.cpp` | Speaker/mic switching, SFX, clap & voice detection, push-to-talk |
| `network_manager.cpp` | Wi-Fi roaming, NTP→RTC, backend calls (core 0) |
| `web_ui.cpp` / `web_page.h` | HTTP server, JSON API, safe OTA, the control panel page |

## Next

Phase 3: AI backend. The hook point is `stopListening()` in `main.cpp`, which should upload the 16 kHz PCM recording to `POST /api/v1/voice`.
