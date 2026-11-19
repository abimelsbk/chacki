# CHACKI — cyberpunk desktop pet for M5Stack CoreS3-SE

Chacki is a small neon robot that lives on your desk. It has moods, needs, a personality and ears. You can control it from your phone, and it runs as an app under [M5 Launcher](https://github.com/bmorcelli/Launcher), next to your other firmwares.

**Website → [abimelsbk.github.io/chacki](https://abimelsbk.github.io/chacki/)** · **Firmware docs → [firmware/chacki-firmware/README.md](firmware/chacki-firmware/README.md)**

## Highlights

- Procedural neon face with 12 moods, blinking, glancing and breathing
- Tamagotchi-style needs: feed it, play ball, pet it
- Personalities: cheerful, sarcastic, sleepy, hacker (matrix rain)
- Built-in skins, plus your own from the SD card
- Reacts to claps and voices
- Pomodoro focus buddy, reminders, alarm, night mode
- Phone web UI for every setting
- Wireless firmware updates
- Push-to-talk with a robot-voice echo; the AI brain backend is coming next

## Quick start

1. Download `Chacki-CoreS3SE-launcher.bin` from [Releases](../../releases), or build it yourself with `pio run` in `firmware/chacki-firmware`.
2. Copy it to your SD card and install it from M5 Launcher.
3. Rename `firmware/chacki-firmware/sdcard/chacki/config.example.txt` to `config.txt`, add your Wi-Fi, and copy the `chacki` folder to the root of the SD card.
4. Swipe up on Chacki to see the web UI address and login.

## Roadmap

- [x] Phase 1–2: firmware, animated character, interaction
- [ ] Phase 3: AI backend (FastAPI: speech-to-text, LLM, text-to-speech, memory)
- [ ] Phase 4: voice conversation on the device
- [ ] Phase 5: wake word "Hey Chacki", long-term memory

## License

Copyright (C) 2026 abimelsbk

This program is free software: you can redistribute it and/or modify it under the terms of the **GNU Affero General Public License v3.0** as published by the Free Software Foundation. See [LICENSE](LICENSE).

If you distribute modified firmware, or run a modified Chacki backend as a network service, you must make your source code available under the same license.

Third-party libraries keep their own licenses:

- [M5Unified](https://github.com/m5stack/M5Unified) / [M5GFX](https://github.com/m5stack/M5GFX): MIT
- [ArduinoJson](https://github.com/bblanchon/ArduinoJson): MIT
- The Orbitron font bundled in M5GFX: SIL OFL
