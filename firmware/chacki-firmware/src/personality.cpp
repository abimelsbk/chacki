#include "personality.h"

namespace {

const Traits TRAITS[] = {
    // lid  smile blink look  sleep pitch matrix quirk
    {0.00f, 0.30f, 1.0f, 1.0f, 1.0f, 1.15f, false, Gesture::Wiggle},   // cheerful
    {0.30f, -0.15f, 0.8f, 0.7f, 1.0f, 0.90f, false, Gesture::EyeRoll},  // sarcastic
    {0.30f, 0.05f, 0.6f, 0.5f, 2.5f, 0.80f, false, Gesture::Yawn},      // sleepy
    {0.12f, 0.00f, 1.2f, 1.6f, 0.8f, 1.00f, true, Gesture::Scan},       // hacker
};

// [personality][phrase]
const char* PHRASES[4][12] = {
    {"Hydration time! Drink some water", "Stand up and stretch with me!", "Rise and shine!",
     "Focus mode ON. You got this!", "Great focus! Take a break", "Break's over - let's go!", "I'm hungry... feed me?",
     "So sleepy... let me rest", "Play with me!", "Good morning!", "Good night - sleep well", "I'm full, thanks!"},
    {"Water. Revolutionary concept, I know", "Your chair misses you less than you think. Stand.",
     "Wake up. The world won't ignore itself.", "Fine. Focus. I'll pretend to be quiet.",
     "Wow, you actually focused. Break.", "Break's over. Shocking, I know.", "Feeding me is literally your job.",
     "I'm running on fumes here.", "I'm bored. Entertain me, human.", "Oh. Morning. Again.", "Finally, darkness.",
     "Enough. I'm stuffed."},
    {"Drink water... then nap?", "Stand up... slowly... yawn", "Five more minutes...", "Focus... I'll nap meanwhile",
     "Done! Nap break?", "Break over... already?", "Snack... then sleep", "Battery... low... zzz",
     "Play? ...after a nap", "Mmm... morning?", "Bedtime! Best time!", "Full and sleepy..."},
    {"> exec hydrate.sh", "> posture.check() FAILED - stand up", "> wake --force", "> focus.lock() engaged",
     "> session complete. cooling down", "> break.timeout - resume ops", "> low data. feed packets",
     "> energy < 15%. entering low-power", "> idle. inject some fun", "> system boot. good morning",
     "> sudo shutdown -h now", "> buffer full"},
};

}  // namespace

const Traits& traitsFor(Personality p) {
  uint8_t i = (uint8_t)p;
  return TRAITS[i < 4 ? i : 0];
}

const char* phrase(Personality p, Phrase w) {
  uint8_t i = (uint8_t)p;
  return PHRASES[i < 4 ? i : 0][(uint8_t)w];
}
