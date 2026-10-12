#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "spoke/status.h"

namespace spoke {

/// Mono 32-bit float PCM. Everything inside spoke is converted to this.
struct AudioBuffer {
  int sample_rate = 16000;
  std::vector<float> samples;  // mono, [-1, 1]

  double durationSec() const;
  size_t numSamples() const { return samples.size(); }
  bool empty() const { return samples.empty(); }
  void clear() { samples.clear(); }
};

Status loadWavFile(const std::string& path, AudioBuffer& out);
Status saveWavFile(const std::string& path, const AudioBuffer& audio);

AudioBuffer resampleAudio(const AudioBuffer& in, int target_rate);
AudioBuffer toMono16k(const AudioBuffer& in);
void applyPreemphasis(AudioBuffer& audio, float coeff = 0.97f);
void normalizePeak(AudioBuffer& audio, float peak = 0.95f);
float rmsEnergy(const AudioBuffer& audio);

// Test helpers (deterministic, no I/O).
AudioBuffer generateSilence(double seconds, int sample_rate = 16000);
AudioBuffer generateSine(double seconds, float freq_hz = 440.0f,
                         float amplitude = 0.5f, int sample_rate = 16000);
AudioBuffer generateNoisySpeechLike(double seconds, int sample_rate = 16000);

}  // namespace spoke
