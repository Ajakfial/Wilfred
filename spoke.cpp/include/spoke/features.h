#pragma once
#include <vector>
#include "spoke/audio.h"
#include "spoke/config.h"

namespace spoke {

/// HTK-style mel filterbank (mel = 2595 log10(1 + f/700)).
class MelFilterbank {
 public:
  MelFilterbank(int n_mels, int n_fft, int sample_rate, float fmin, float fmax);
  int nMels() const { return n_mels_; }
  int nFft() const { return n_fft_; }
  /// power: [(n_fft/2)+1] linear power spectrum -> [n_mels] mel energies.
  void apply(const float* power, float* mel_out) const;
  std::vector<float> apply(const std::vector<float>& power) const;

 private:
  int n_mels_, n_fft_, sample_rate_;
  // Sparse representation: for each mel bin, [start,end) + weights.
  std::vector<int> row_start_;
  std::vector<int> row_end_;
  std::vector<float> weights_;  // concatenated per-row weights
  std::vector<size_t> row_offset_;
};

/// Log-mel frontend: framing -> Hann -> FFT(512) -> mel -> log -> CMVN.
/// Output: [num_frames x n_mels], ~100 frames/sec @ 10 ms hop.
class LogMelExtractor {
 public:
  explicit LogMelExtractor(const SpokeConfig& config);

  std::vector<std::vector<float>> extract(const AudioBuffer& audio) const;
  size_t numFrames(size_t num_samples) const;

  const SpokeConfig& config() const { return config_; }

 private:
  SpokeConfig config_;
};

void applyCmvn(std::vector<std::vector<float>>& feats, float eps = 1e-5f);
void appendDeltas(const std::vector<std::vector<float>>& feats,
                  std::vector<std::vector<float>>& out_with_deltas);

}  // namespace spoke
