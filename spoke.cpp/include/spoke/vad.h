#pragma once
#include <vector>
#include "spoke/audio.h"

namespace spoke {

struct VadSegment {
  double start_sec = 0.0;
  double end_sec = 0.0;
};

struct VadConfig {
  float energy_threshold_db = -38.0f;
  int frame_ms = 30;
  int hangover_frames = 6;   // keep speech through brief dips
  double min_speech_sec = 0.25;
  double min_silence_sec = 0.20;
};

/// Energy + zero-crossing VAD, from scratch. No neural VAD needed:
/// speech has higher energy AND moderate ZCR vs silence/noise.
class EnergyVad {
 public:
  explicit EnergyVad(VadConfig cfg = VadConfig{});

  std::vector<VadSegment> detect(const AudioBuffer& audio) const;
  std::vector<bool> frameVoiced(const AudioBuffer& audio) const;

 private:
  VadConfig cfg_;
  static float frameEnergyDb(const float* x, size_t n);
  static float zeroCrossingRate(const float* x, size_t n);
};

}  // namespace spoke
