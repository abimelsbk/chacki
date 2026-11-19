#include "audio_manager.h"

#include <M5Unified.h>
#include <math.h>

#include "config.h"

namespace {

constexpr size_t CHUNK = 256;  // samples per mic request (16 ms @ 16 kHz)
constexpr size_t RING_SLOTS = 4;
constexpr size_t ENV_BLOCK = 256;
constexpr uint32_t HOLDOFF_MS = 300;  // ignore the mic right after we made noise

// (frequency Hz, duration ms) pairs; 1 = rest; terminated by {0,0}.
const uint16_t SEQ_BOOT[] = {523, 70, 659, 70, 784, 70, 1047, 150, 0, 0};
const uint16_t SEQ_TAP[] = {1600, 25, 0, 0};
const uint16_t SEQ_HAPPY[] = {880, 60, 1175, 60, 1568, 110, 0, 0};
const uint16_t SEQ_ANGRY[] = {240, 110, 1, 30, 180, 180, 0, 0};
const uint16_t SEQ_SAD[] = {660, 160, 520, 160, 400, 280, 0, 0};
const uint16_t SEQ_CURIOUS[] = {700, 60, 1100, 120, 0, 0};
const uint16_t SEQ_LISTEN[] = {1320, 45, 1760, 45, 0, 0};
const uint16_t SEQ_DONE[] = {1760, 40, 1320, 50, 0, 0};
const uint16_t SEQ_WAKE[] = {392, 60, 587, 60, 784, 90, 0, 0};
const uint16_t SEQ_SLEEP[] = {784, 120, 587, 140, 392, 220, 0, 0};
const uint16_t SEQ_PURR[] = {130, 70, 1, 20, 150, 70, 1, 20, 130, 90, 0, 0};
const uint16_t SEQ_ERROR[] = {200, 150, 1, 40, 150, 260, 0, 0};
const uint16_t SEQ_SWIPE[] = {900, 25, 1400, 25, 0, 0};
const uint16_t SEQ_CHOMP[] = {300, 50, 1, 70, 260, 50, 1, 70, 320, 50, 1, 70, 280, 60, 0, 0};
const uint16_t SEQ_PLAY[] = {1047, 50, 1319, 50, 1568, 50, 2093, 100, 0, 0};
const uint16_t SEQ_ALARM[] = {1760, 90, 1, 60, 1760, 90, 1, 60, 1760, 90, 1, 60, 2093, 250, 0, 0};
const uint16_t SEQ_REMIND[] = {1175, 80, 1568, 80, 1175, 80, 1568, 140, 0, 0};
const uint16_t SEQ_FSTART[] = {523, 90, 784, 90, 1047, 160, 0, 0};
const uint16_t SEQ_FDONE[] = {784, 80, 988, 80, 1175, 80, 1568, 80, 2093, 220, 0, 0};
const uint16_t SEQ_STARTLE[] = {2400, 30, 1800, 30, 2600, 50, 0, 0};
const uint16_t SEQ_BLEEP[] = {1900, 20, 1, 20, 2300, 20, 1, 20, 1700, 30, 0, 0};

const uint16_t* seqFor(Sfx s) {
  switch (s) {
    case Sfx::Boot: return SEQ_BOOT;
    case Sfx::Tap: return SEQ_TAP;
    case Sfx::Happy: return SEQ_HAPPY;
    case Sfx::Angry: return SEQ_ANGRY;
    case Sfx::Sad: return SEQ_SAD;
    case Sfx::Curious: return SEQ_CURIOUS;
    case Sfx::Listen: return SEQ_LISTEN;
    case Sfx::Done: return SEQ_DONE;
    case Sfx::Wake: return SEQ_WAKE;
    case Sfx::Sleep: return SEQ_SLEEP;
    case Sfx::Purr: return SEQ_PURR;
    case Sfx::Error: return SEQ_ERROR;
    case Sfx::Swipe: return SEQ_SWIPE;
    case Sfx::Chomp: return SEQ_CHOMP;
    case Sfx::Play: return SEQ_PLAY;
    case Sfx::Alarm: return SEQ_ALARM;
    case Sfx::Reminder: return SEQ_REMIND;
    case Sfx::FocusStart: return SEQ_FSTART;
    case Sfx::FocusDone: return SEQ_FDONE;
    case Sfx::Startle: return SEQ_STARTLE;
    case Sfx::Bleep: return SEQ_BLEEP;
  }
  return nullptr;
}

float rms(const int16_t* p, size_t n) {
  if (!n) return 0.f;
  int64_t mean = 0;
  for (size_t i = 0; i < n; i++) mean += p[i];
  mean /= (int64_t)n;
  double acc = 0;
  for (size_t i = 0; i < n; i++) {
    double v = p[i] - mean;
    acc += v * v;
  }
  return sqrtf((float)(acc / n));
}

}  // namespace

bool AudioManager::begin(uint8_t volume) {
  cap_ = (size_t)MIC_SAMPLE_RATE * MAX_RECORD_MS / 1000;
  buf_ = (int16_t*)heap_caps_malloc(cap_ * sizeof(int16_t), MALLOC_CAP_SPIRAM);
  env_ = (uint8_t*)heap_caps_malloc(cap_ / ENV_BLOCK + 1, MALLOC_CAP_SPIRAM);
  ring_ = (int16_t*)heap_caps_malloc(CHUNK * RING_SLOTS * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  volume_ = volume;
  toSpeaker();
  return buf_ && env_ && ring_;
}

void AudioManager::setVolume(uint8_t v) {
  volume_ = v;
  if (mode_ == Mode::Speaker) M5.Speaker.setVolume(v);
}

void AudioManager::setListening(bool on, uint8_t sensitivity) {
  listenWanted_ = on;
  sens_ = sensitivity < 1 ? 1 : (sensitivity > 10 ? 10 : sensitivity);
}

// ------------------------------------------------------------ mode switches --

void AudioManager::stopMicQueue() {
  uint32_t t0 = millis();
  while (M5.Mic.isRecording() && millis() - t0 < 100) delay(1);
}

void AudioManager::toSpeaker() {
  if (mode_ != Mode::Speaker) {
    stopMicQueue();
    M5.Mic.end();
  }
  mode_ = Mode::Speaker;
  if (!M5.Speaker.isEnabled()) M5.Speaker.begin();
  M5.Speaker.setVolume(volume_);
}

void AudioManager::toMic() {
  if (mode_ != Mode::Speaker) return;
  M5.Speaker.stop();
  M5.Speaker.end();
  M5.Mic.begin();
}

bool AudioManager::busySpeaker() const { return seq_ != nullptr || playingVoice_ || M5.Speaker.isPlaying(); }

// ------------------------------------------------------------ effects -------

void AudioManager::play(Sfx s, bool force) {
  if (mode_ == Mode::Record) return;
  if (!sfxOn_ && !force) return;
  if (mode_ == Mode::Listen) toSpeaker();
  seq_ = seqFor(s);
  seqNext_ = 0;
}

void AudioManager::update() {
  uint32_t now = millis();

  if (seq_ && mode_ == Mode::Speaker && (int32_t)(now - seqNext_) >= 0) {
    uint16_t f = seq_[0], ms = seq_[1];
    if (f == 0) {
      seq_ = nullptr;
      holdoffUntil_ = now + HOLDOFF_MS;
    } else {
      if (f > 1 && !playingVoice_) M5.Speaker.tone(f * pitch_, ms, 1);
      seqNext_ = now + ms + 8;
      seq_ += 2;
    }
  }

  if (playingVoice_ && !M5.Speaker.isPlaying()) {
    playingVoice_ = false;
    holdoffUntil_ = now + HOLDOFF_MS;
  }

  switch (mode_) {
    case Mode::Speaker:
      if (listenWanted_ && !busySpeaker() && (int32_t)(now - holdoffUntil_) >= 0) {
        toMic();
        mode_ = Mode::Listen;
        issued_ = processed_ = 0;
        prevRms_ = 0;
        holdoffUntil_ = now + 150;  // mic settling
      }
      break;
    case Mode::Listen:
      if (!listenWanted_) {
        toSpeaker();
        break;
      }
      feedListen();
      break;
    case Mode::Record: {
      while (pos_ + CHUNK <= cap_ && M5.Mic.isRecording() < 2) {
        M5.Mic.record(buf_ + pos_, CHUNK, MIC_SAMPLE_RATE);
        pos_ += CHUNK;
      }
      size_t pending = M5.Mic.isRecording();
      if (pos_ >= (pending + 1) * CHUNK) {
        float lvl = rms(buf_ + pos_ - (pending + 1) * CHUNK, CHUNK) / 3000.f;
        micLevel_ += ((lvl > 1.f ? 1.f : lvl) - micLevel_) * 0.5f;
      }
      if (pos_ + CHUNK > cap_ && pending == 0) stopRecording();
      break;
    }
  }
}

// ------------------------------------------------------------ listening -----

void AudioManager::feedListen() {
  uint32_t now = millis();
  size_t pending = M5.Mic.isRecording();
  uint32_t done = issued_ - pending;
  while (processed_ < done) {
    const int16_t* chunk = ring_ + (processed_ % RING_SLOTS) * CHUNK;
    if ((int32_t)(now - holdoffUntil_) >= 0) detect(rms(chunk, CHUNK), now);
    processed_++;
  }
  while (M5.Mic.isRecording() < 2) {
    if (!M5.Mic.record(ring_ + (issued_ % RING_SLOTS) * CHUNK, CHUNK, MIC_SAMPLE_RATE)) break;
    issued_++;
  }
}

void AudioManager::detect(float r, uint32_t now) {
  float lvl = r / 3000.f;
  ambient_ += ((lvl > 1.f ? 1.f : lvl) - ambient_) * 0.3f;
  micLevel_ = ambient_;

  // sensitivity 1..10 -> clap threshold 5500..1000 (raw RMS)
  const float clapThr = 6000.f - sens_ * 500.f;
  const float voiceThr = clapThr * 0.3f;

  bool sharp = r > clapThr && r > bg_ * 4.f && r > prevRms_ * 2.2f;
  prevRms_ = r;

  // resolve a pending single clap
  if (firstClapAt_ && now - firstClapAt_ > 650) {
    firstClapAt_ = 0;
    pending_ = AudioEvent::Clap;
    cooldownUntil_ = now + 600;
  }

  if (sharp && (int32_t)(now - cooldownUntil_) >= 0) {
    if (firstClapAt_ && now - firstClapAt_ > 100) {
      firstClapAt_ = 0;
      pending_ = AudioEvent::DoubleClap;
      cooldownUntil_ = now + 1200;
    } else if (!firstClapAt_) {
      firstClapAt_ = now;
    }
    loudChunks_ = 0;
    return;
  }

  // sustained sound well above the room -> someone talking nearby
  if (r > voiceThr && r > bg_ * 2.5f) {
    if (++loudChunks_ >= 40 && (int32_t)(now - voiceCooldown_) >= 0) {
      pending_ = AudioEvent::Voice;
      voiceCooldown_ = now + 20000;
      loudChunks_ = 0;
    }
  } else {
    loudChunks_ = loudChunks_ > 2 ? loudChunks_ - 2 : 0;
    bg_ += (r - bg_) * 0.02f;  // learn the room's background noise
    if (bg_ < 50) bg_ = 50;
  }
}

AudioEvent AudioManager::takeEvent() {
  AudioEvent e = pending_;
  pending_ = AudioEvent::None;
  return e;
}

// ------------------------------------------------------------ recording -----

bool AudioManager::startRecording() {
  if (!buf_) return false;
  seq_ = nullptr;
  playingVoice_ = false;
  if (mode_ == Mode::Listen) {
    stopMicQueue();
  } else {
    while (M5.Speaker.isPlaying()) delay(1);
    toMic();
  }
  mode_ = Mode::Record;
  pos_ = len_ = 0;
  micLevel_ = 0.f;
  firstClapAt_ = 0;
  pending_ = AudioEvent::None;
  return true;
}

uint32_t AudioManager::stopRecording() {
  if (mode_ != Mode::Record) return (uint32_t)(len_ * 1000 / MIC_SAMPLE_RATE);
  stopMicQueue();
  len_ = pos_;
  micLevel_ = 0.f;
  mode_ = Mode::Listen;  // so toSpeaker() shuts the mic down
  toSpeaker();
  return (uint32_t)(len_ * 1000 / MIC_SAMPLE_RATE);
}

bool AudioManager::playRecordingAsRobot() {
  if (!buf_ || len_ < CHUNK * 4) return false;
  if (mode_ != Mode::Speaker) toSpeaker();

  int64_t mean = 0;
  for (size_t i = 0; i < len_; i++) mean += buf_[i];
  mean /= (int64_t)len_;
  const float w = 2.f * (float)M_PI * 55.f / MIC_SAMPLE_RATE;
  float peak = 1.f;
  for (size_t i = 0; i < len_; i++) {
    float v = (float)(buf_[i] - mean);
    v = 0.35f * v + 0.65f * v * sinf(w * i);  // ring modulation = robot timbre
    if (fabsf(v) > peak) peak = fabsf(v);
    buf_[i] = (int16_t)fmaxf(-32767.f, fminf(32767.f, v));
  }
  float gain = 26000.f / peak;
  if (gain > 12.f) gain = 12.f;
  for (size_t i = 0; i < len_; i++) buf_[i] = (int16_t)fmaxf(-32767.f, fminf(32767.f, buf_[i] * gain));

  envLen_ = len_ / ENV_BLOCK;
  float envPeak = 1.f;
  for (size_t b = 0; b < envLen_; b++) envPeak = fmaxf(envPeak, rms(buf_ + b * ENV_BLOCK, ENV_BLOCK));
  for (size_t b = 0; b < envLen_; b++)
    env_[b] = (uint8_t)(fminf(1.f, rms(buf_ + b * ENV_BLOCK, ENV_BLOCK) / envPeak * 1.3f) * 255);

  playRate_ = (uint32_t)(MIC_SAMPLE_RATE * 1.12f);
  M5.Speaker.playRaw(buf_, len_, playRate_, false, 1, 0, true);
  playStart_ = millis();
  playingVoice_ = true;
  return true;
}

float AudioManager::speakLevel() const {
  if (!playingVoice_ || envLen_ == 0) return 0.f;
  size_t b = (size_t)((uint64_t)(millis() - playStart_) * playRate_ / 1000) / ENV_BLOCK;
  return b >= envLen_ ? 0.f : env_[b] / 255.f;
}
