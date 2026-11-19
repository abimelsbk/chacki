#pragma once
#include <stddef.h>
#include <stdint.h>

enum class Sfx : uint8_t {
  Boot, Tap, Happy, Angry, Sad, Curious, Listen, Done, Wake, Sleep, Purr, Error, Swipe,
  Chomp, Play, Alarm, Reminder, FocusStart, FocusDone, Startle, Bleep
};

enum class AudioEvent : uint8_t { None, Clap, DoubleClap, Voice };

// The CoreS3 speaker and microphone share one I2S bus, so this class juggles
// three modes:
//   speaker  - sound effects / voice playback
//   listen   - mic runs in the background for clap & voice detection
//   record   - push-to-talk capture into a PSRAM buffer
class AudioManager {
 public:
  bool begin(uint8_t volume);
  void update();
  void setVolume(uint8_t v);
  void setSfxEnabled(bool on) { sfxOn_ = on; }
  void setPitch(float p) { pitch_ = p; }
  void setListening(bool on, uint8_t sensitivity);

  void play(Sfx s, bool force = false);  // force: plays even if SFX are muted

  bool startRecording();
  uint32_t stopRecording();  // returns recorded ms
  bool recording() const { return mode_ == Mode::Record; }
  float micLevel() const { return micLevel_; }      // 0..1, record or listen
  float ambientLevel() const { return ambient_; }  // 0..1 for the web UI meter

  AudioEvent takeEvent();

  bool playRecordingAsRobot();
  bool speaking() const { return playingVoice_; }
  float speakLevel() const;

 private:
  enum class Mode : uint8_t { Speaker, Listen, Record };
  void toSpeaker();
  void toMic();
  void stopMicQueue();
  void feedListen();
  void detect(float rms, uint32_t now);
  bool busySpeaker() const;

  Mode mode_ = Mode::Speaker;
  int16_t* buf_ = nullptr;
  size_t cap_ = 0, len_ = 0, pos_ = 0;
  uint8_t* env_ = nullptr;
  size_t envLen_ = 0;

  int16_t* ring_ = nullptr;  // listen-mode ring of RING_SLOTS chunks
  uint32_t issued_ = 0, processed_ = 0;

  bool listenWanted_ = false;
  uint8_t sens_ = 6;
  uint32_t holdoffUntil_ = 0;
  float bg_ = 300.f, prevRms_ = 0.f;
  uint32_t firstClapAt_ = 0, cooldownUntil_ = 0, voiceCooldown_ = 0;
  uint16_t loudChunks_ = 0;
  AudioEvent pending_ = AudioEvent::None;

  float micLevel_ = 0.f, ambient_ = 0.f;
  uint32_t playStart_ = 0, playRate_ = 0;
  bool playingVoice_ = false;
  bool sfxOn_ = true;
  float pitch_ = 1.f;
  uint8_t volume_ = 150;

  const uint16_t* seq_ = nullptr;
  uint32_t seqNext_ = 0;
};
