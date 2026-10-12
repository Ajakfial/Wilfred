#include "spoke/vad.h"
#include <algorithm>
#include <cmath>

namespace spoke {

EnergyVad::EnergyVad(VadConfig cfg) : cfg_(cfg) {}

float EnergyVad::frameEnergyDb(const float* x, size_t n) {
  double acc = 0;
  for (size_t i = 0; i < n; ++i) acc += static_cast<double>(x[i]) * x[i];
  double rms = std::sqrt(acc / (n > 0 ? n : 1));
  if (rms < 1e-9) return -120.0f;
  return static_cast<float>(20.0 * std::log10(rms));
}

float EnergyVad::zeroCrossingRate(const float* x, size_t n) {
  if (n < 2) return 0.0f;
  size_t zc = 0;
  for (size_t i = 1; i < n; ++i)
    if ((x[i] >= 0) != (x[i - 1] >= 0)) ++zc;
  return static_cast<float>(zc) / static_cast<float>(n - 1);
}

std::vector<bool> EnergyVad::frameVoiced(const AudioBuffer& audio) const {
  std::vector<bool> voiced;
  if (audio.samples.empty() || audio.sample_rate <= 0) return voiced;
  size_t frame_n = static_cast<size_t>(audio.sample_rate * cfg_.frame_ms / 1000);
  if (frame_n == 0) frame_n = 480;
  size_t n_frames = (audio.samples.size() + frame_n - 1) / frame_n;
  voiced.assign(n_frames, false);
  for (size_t f = 0; f < n_frames; ++f) {
    size_t off = f * frame_n;
    size_t len = std::min(frame_n, audio.samples.size() - off);
    float e = frameEnergyDb(audio.samples.data() + off, len);
    float zcr = zeroCrossingRate(audio.samples.data() + off, len);
    // Speech: above energy floor AND not pathological ZCR (pure hiss).
    voiced[f] = (e > cfg_.energy_threshold_db) && (zcr < 0.45f);
  }
  // Hangover: bridge short gaps so words aren't chopped.
  for (int pass = 0; pass < cfg_.hangover_frames; ++pass) {
    std::vector<bool> smoothed = voiced;
    for (size_t i = 1; i + 1 < voiced.size(); ++i)
      if (!voiced[i] && voiced[i - 1] && voiced[i + 1]) smoothed[i] = true;
    voiced.swap(smoothed);
  }
  return voiced;
}

std::vector<VadSegment> EnergyVad::detect(const AudioBuffer& audio) const {
  std::vector<VadSegment> segs;
  auto voiced = frameVoiced(audio);
  if (voiced.empty()) return segs;
  double frame_sec = static_cast<double>(cfg_.frame_ms) / 1000.0;
  bool in_speech = false;
  size_t start = 0;
  for (size_t i = 0; i <= voiced.size(); ++i) {
    bool v = (i < voiced.size()) ? voiced[i] : false;
    if (v && !in_speech) {
      in_speech = true;
      start = i;
    } else if (!v && in_speech) {
      in_speech = false;
      double s = start * frame_sec, e = i * frame_sec;
      if (e - s >= cfg_.min_speech_sec) segs.push_back({s, e});
    }
  }
  // If nothing voiced but audio is long & loud, keep whole file (avoid
  // dropping valid quiet recordings when threshold is strict).
  if (segs.empty() && audio.durationSec() > 0.5) {
    float e = frameEnergyDb(audio.samples.data(), audio.samples.size());
    if (e > cfg_.energy_threshold_db - 12.0f)
      segs.push_back({0.0, audio.durationSec()});
  }
  return segs;
}

}  // namespace spoke
