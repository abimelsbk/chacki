#include "skins.h"

#include <SD.h>

SkinLibrary gSkins;

void SkinLibrary::begin() {
  skins_.clear();
  skins_.push_back({"Neon Cyan", 0x00E6FF, 0xAA46FF, 0x040612, EyeShape::Round, true, false});
  skins_.push_back({"Synthwave", 0xFF3CAA, 0x00E6FF, 0x12041C, EyeShape::Round, true, false});
  skins_.push_back({"Matrix", 0x28FF6E, 0x0F8F3A, 0x000A02, EyeShape::Square, true, false});
  skins_.push_back({"Amber Term", 0xFFB428, 0xFF6A00, 0x0C0700, EyeShape::Square, true, false});
  skins_.push_back({"Ice", 0xD8F4FF, 0x6AB8FF, 0x050A14, EyeShape::Pill, false, false});
  skins_.push_back({"Blood Moon", 0xFF2840, 0x8A0018, 0x0A0002, EyeShape::Pill, true, false});
  skins_.push_back({"Toxic", 0xC6FF00, 0xFF00C8, 0x080A00, EyeShape::Round, true, false});
}

const Skin* SkinLibrary::find(const String& name) const {
  for (auto& s : skins_)
    if (s.name.equalsIgnoreCase(name)) return &s;
  return nullptr;
}

void SkinLibrary::applyTo(const Skin& s, Settings& st) const {
  st.skinName = s.name;
  st.eyeColor = s.eye;
  st.accent = s.accent;
  st.bgColor = s.bg;
  st.eyeShape = s.shape;
  st.scanlines = s.scanlines;
}

void SkinLibrary::loadFromSd() {
  File dir = SD.open("/chacki/skins");
  if (!dir || !dir.isDirectory()) return;
  for (File f = dir.openNextFile(); f && skins_.size() < 24; f = dir.openNextFile()) {
    String fname = f.name();
    if (f.isDirectory() || !fname.endsWith(".txt")) continue;
    Skin s{"", 0x00E6FF, 0xAA46FF, 0x040612, EyeShape::Round, true, true};
    int slash = fname.lastIndexOf('/');
    s.name = fname.substring(slash + 1, fname.length() - 4);
    while (f.available()) {
      String line = f.readStringUntil('\n');
      line.trim();
      int eq = line.indexOf('=');
      if (line.startsWith("#") || eq <= 0) continue;
      String k = line.substring(0, eq), v = line.substring(eq + 1);
      k.trim();
      v.trim();
      k.toLowerCase();
      if (k == "name") s.name = v.substring(0, 20);
      else if (k == "eye") s.eye = parseHexColor(v, s.eye);
      else if (k == "accent") s.accent = parseHexColor(v, s.accent);
      else if (k == "bg") s.bg = parseHexColor(v, s.bg);
      else if (k == "scanlines") s.scanlines = v != "0" && !v.equalsIgnoreCase("false");
      else if (k == "shape") {
        for (uint8_t i = 0; i < (uint8_t)EyeShape::COUNT; i++)
          if (v.equalsIgnoreCase(eyeShapeName((EyeShape)i))) s.shape = (EyeShape)i;
      }
    }
    if (s.name.length() && !find(s.name)) skins_.push_back(s);
  }
}
