#include "spoke/features.h"
#include "spoke/fft.h"
#include <algorithm>
#include <cmath>

namespace spoke {

static float hzToMel(float hz) { return 2595.0f * std::log10(1.0f + hz / 700.0f); }
static float melToHz(float mel) { return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f); }

MelFilterbank::MelFilterbank(int n_mels, int n_fft, int sample_rate, float fmin,
                             float fmax)
    : n_mels_(n_mels), n_fft_(n_fft), sample_rate_(sample_rate) {
  int n_fft_pow = static_cast<int>(Fft::nextPowerOfTwo(static_cast<size_t>(n_fft)));
  int n_bins = n_fft_pow / 2 + 1;
  float mel_lo = hzToMel(fmin), mel_hi = hzToMel(fmax);
  std::vector<float> mel_pts(n_mels + 2);
  for (int i = 0; i < n_mels + 2; ++i)
    mel_pts[i] = mel_lo + (mel_hi - mel_lo) * i / (n_mels + 1);
  std::vector<float> hz_pts(n_mels + 2);
  for (int i = 0; i < n_mels + 2; ++i) hz_pts[i] = melToHz(mel_pts[i]);
  // FFT bin center frequencies.
  float bin_hz = static_cast<float>(sample_rate) / n_fft_pow;

  row_start_.assign(n_mels, 0);
  row_end_.assign(n_mels, 0);
  row_offset_.assign(n_mels + 1, 0);
  std::vector<float> all;
  all.reserve(static_cast<size_t>(n_mels * 16));
  for (int m = 0; m < n_mels; ++m) {
    float lo = hz_pts[m], ce = hz_pts[m + 1], hi = hz_pts[m + 2];
    int start = static_cast<int>(std::floor(lo / bin_hz));
    int end = static_cast<int>(std::ceil(hi / bin_hz)) + 1;
    start = std::max(0, start);
    end = std::min(n_bins, end);
    row_start_[m] = start;
    row_end_[m] = end;
    row_offset_[m] = all.size();
    for (int k = start; k < end; ++k) {
      float f = k * bin_hz;
      float w = 0.0f;
      if (f >= lo && f <= ce && ce > lo) w = (f - lo) / (ce - lo);
      else if (f >= ce && f <= hi && hi > ce) w = (hi - f) / (hi - ce);
      all.push_back(w);
    }
  }
  row_offset_[n_mels] = all.size();
  weights_ = std::move(all);
}

void MelFilterbank::apply(const float* power, float* mel_out) const {
  for (int m = 0; m < n_mels_; ++m) {
    float acc = 0.0f;
    size_t off = row_offset_[m];
    for (int k = row_start_[m]; k < row_end_[m]; ++k) acc += power[k] * weights_[off++];
    mel_out[m] = acc;
  }
}

std::vector<float> MelFilterbank::apply(const std::vector<float>& power) const {
  std::vector<float> out(n_mels_);
  apply(power.data(), out.data());
  return out;
}

LogMelExtractor::LogMelExtractor(const SpokeConfig& config) : config_(config) {}

size_t LogMelExtractor::numFrames(size_t num_samples) const {
  int win = config_.win_length, hop = config_.hop_length;
  if (num_samples < static_cast<size_t>(win)) return 0;
  return 1 + (num_samples - win) / hop;
}

std::vector<std::vector<float>> LogMelExtractor::extract(const AudioBuffer& audio) const {
  int win = config_.win_length, hop = config_.hop_length;
  int n_fft_pow = static_cast<int>(Fft::nextPowerOfTwo(static_cast<size_t>(config_.n_fft)));
  size_t n = audio.samples.size();
  size_t n_frames = numFrames(n);
  std::vector<std::vector<float>> feats;
  feats.reserve(n_frames);
  if (n_frames == 0) return feats;

  Fft fft(static_cast<size_t>(n_fft_pow));
  MelFilterbank mel(config_.n_mels, config_.n_fft, config_.sample_rate, config_.fmin,
                    config_.fmax);
  std::vector<float> window = hannWindow(static_cast<size_t>(win));
  std::vector<float> frame(static_cast<size_t>(n_fft_pow));
  std::vector<float> power(static_cast<size_t>(n_fft_pow / 2 + 1));
  std::vector<float> mel_out(static_cast<size_t>(config_.n_mels));

  const float floor_val = 1e-10f;
  for (size_t f = 0; f < n_frames; ++f) {
    size_t off = f * hop;
    for (int i = 0; i < n_fft_pow; ++i) {
      float s = 0.0f;
      if (i < win && off + i < n) s = audio.samples[off + i] * window[i];
      frame[i] = s;
    }
    fft.powerSpectrum(frame.data(), power.data());
    mel.apply(power.data(), mel_out.data());
    std::vector<float> logmel(config_.n_mels);
    for (int m = 0; m < config_.n_mels; ++m)
      logmel[m] = std::log(std::max(mel_out[m], floor_val));
    feats.push_back(std::move(logmel));
  }
  if (config_.use_cmvn) applyCmvn(feats);
  return feats;
}

void applyCmvn(std::vector<std::vector<float>>& feats, float eps) {
  if (feats.empty()) return;
  size_t T = feats.size(), D = feats[0].size();
  std::vector<double> mean(D, 0.0);
  for (auto& fr : feats)
    for (size_t d = 0; d < D; ++d) mean[d] += fr[d];
  for (size_t d = 0; d < D; ++d) mean[d] /= T;
  std::vector<double> var(D, 0.0);
  for (auto& fr : feats)
    for (size_t d = 0; d < D; ++d) {
      double dd = fr[d] - mean[d];
      var[d] += dd * dd;
    }
  for (size_t d = 0; d < D; ++d) var[d] /= T;
  for (auto& fr : feats)
    for (size_t d = 0; d < D; ++d)
      fr[d] = static_cast<float>((fr[d] - mean[d]) / std::sqrt(var[d] + eps));
}

void appendDeltas(const std::vector<std::vector<float>>& feats,
                  std::vector<std::vector<float>>& out_with_deltas) {
  if (feats.empty()) {
    out_with_deltas.clear();
    return;
  }
  size_t T = feats.size(), D = feats[0].size();
  out_with_deltas.assign(T, std::vector<float>(D * 3, 0.0f));
  auto delta_at = [&](size_t t, size_t d) -> float {
    float prev = feats[t > 0 ? t - 1 : 0][d];
    float next = feats[t + 1 < T ? t + 1 : T - 1][d];
    return (next - prev) * 0.5f;
  };
  for (size_t t = 0; t < T; ++t)
    for (size_t d = 0; d < D; ++d) {
      float v = feats[t][d];
      float d1 = delta_at(t, d);
      // Second-order delta from d1 neighbours (approx).
      float d1p = (t > 0) ? ((feats[t][d] - feats[t - 1][d])) : d1;
      float d1n = (t + 1 < T) ? ((feats[t + 1][d] - feats[t][d])) : d1;
      float d2 = (d1n - d1p) * 0.5f;
      out_with_deltas[t][d] = v;
      out_with_deltas[t][D + d] = d1;
      out_with_deltas[t][2 * D + d] = d2;
    }
}

}  // namespace spoke
