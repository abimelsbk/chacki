#pragma once
#include <Arduino.h>
#include <vector>

#include "settings.h"

struct Skin {
  String name;
  uint32_t eye, accent, bg;
  EyeShape shape;
  bool scanlines;
  bool custom;  // loaded from SD
};

// Built-in skins plus any /chacki/skins/*.txt files on the SD card:
//   name=Vaporwave
//   eye=#ff71ce
//   accent=#01cdfe
//   bg=#1a0033
//   shape=round|square|pill
//   scanlines=1
class SkinLibrary {
 public:
  void begin();
  void loadFromSd();  // SD must already be mounted
  size_t count() const { return skins_.size(); }
  const Skin& at(size_t i) const { return skins_[i]; }
  const Skin* find(const String& name) const;
  void applyTo(const Skin& s, Settings& st) const;

 private:
  std::vector<Skin> skins_;
};

extern SkinLibrary gSkins;
