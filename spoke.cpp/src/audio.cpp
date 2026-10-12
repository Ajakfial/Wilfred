#include "spoke/audio.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace spoke {

double AudioBuffer::durationSec() const {
  if (sample_rate <= 0) return 0.0;
  return static_cast<double>(samples.size()) / sample_rate;
}

// --- WAV I/O (PCM16 + IEEE float, mono/stereo) --------------------------------

#pragma pack(push, 1)
struct WavHeader {
  char riff[4];         // "RIFF"
  uint32_t chunk_size;
  char wave[4];         // "WAVE"
  char fmt[4];          // "fmt "
  uint32_t fmt_size;    // 16
  uint16_t audio_fmt;   // 1=PCM, 3=float
  uint16_t channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
};
#pragma pack(pop)

Status loadWavFile(const std::string& path, AudioBuffer& out) {
  FILE* f = nullptr;
// fopen_s is MSVC-only; every other toolchain (GCC, Clang, MinGW, AppleClang)
// uses plain fopen. Guard on _MSC_VER, not _WIN32, so MinGW builds work too.
#if defined(_MSC_VER)
  fopen_s(&f, path.c_str(), "rb");
#else
  f = std::fopen(path.c_str(), "rb");
#endif
  if (!f) return Status::Error(StatusCode::IoError, "cannot open file: " + path);

  WavHeader h{};
  if (std::fread(&h, sizeof(h), 1, f) != 1) {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat, "cannot read WAV header");
  }
  if (std::memcmp(h.riff, "RIFF", 4) != 0 || std::memcmp(h.wave, "WAVE", 4) != 0) {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat, "not a WAV file");
  }
  // Validate the format header before trusting any size fields below: a
  // corrupt file with channels == 0 or bits_per_sample == 0 would otherwise
  // divide by zero when computing the frame count (process crash, not error).
  if (h.channels == 0 || h.channels > 32) {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat, "unsupported channel count");
  }
  if (h.sample_rate <= 0 || h.sample_rate > 384000) {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat, "unsupported sample rate");
  }
  // Only the combos the readers below handle (PCM8 / PCM16 / float32).
  bool fmt_ok = (h.audio_fmt == 1 && (h.bits_per_sample == 8 || h.bits_per_sample == 16)) ||
                (h.audio_fmt == 3 && h.bits_per_sample == 32);
  if (!fmt_ok) {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat,
                         "only PCM8/PCM16/float32 WAV supported");
  }
  // Skip any extra fmt bytes.
  if (h.fmt_size > 16) std::fseek(f, static_cast<long>(h.fmt_size - 16), SEEK_CUR);

  // Find "data" chunk (skip fact/list/etc).
  char chunk_id[4];
  uint32_t chunk_size = 0;
  bool found = false;
  for (int i = 0; i < 32; ++i) {
    if (std::fread(chunk_id, 4, 1, f) != 1) break;
    if (std::fread(&chunk_size, 4, 1, f) != 1) break;
    if (std::memcmp(chunk_id, "data", 4) == 0) {
      found = true;
      break;
    }
    std::fseek(f, static_cast<long>(chunk_size), SEEK_CUR);
  }
  if (!found) {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat, "WAV data chunk not found");
  }
  // Cap absurd data chunks from lying headers before any allocation.
  constexpr uint32_t kMaxWavDataBytes = 512u * 1024u * 1024u;
  if (chunk_size > kMaxWavDataBytes) {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat, "WAV data chunk too large");
  }

  size_t bytes_per_sample = h.bits_per_sample / 8;
  size_t num_frames = chunk_size / (bytes_per_sample * h.channels);
  std::vector<float> mono;
  mono.reserve(num_frames);

  if (h.audio_fmt == 1 && h.bits_per_sample == 16) {
    std::vector<int16_t> tmp(num_frames * h.channels);
    size_t got = std::fread(tmp.data(), sizeof(int16_t), tmp.size(), f);
    (void)got;
    for (size_t i = 0; i < num_frames; ++i) {
      double acc = 0;
      for (int c = 0; c < h.channels; ++c) acc += tmp[i * h.channels + c] / 32768.0;
      mono.push_back(static_cast<float>(acc / h.channels));
    }
  } else if (h.audio_fmt == 3 && h.bits_per_sample == 32) {
    std::vector<float> tmp(num_frames * h.channels);
    size_t got = std::fread(tmp.data(), sizeof(float), tmp.size(), f);
    (void)got;
    for (size_t i = 0; i < num_frames; ++i) {
      double acc = 0;
      for (int c = 0; c < h.channels; ++c) acc += tmp[i * h.channels + c];
      mono.push_back(static_cast<float>(acc / h.channels));
    }
  } else if (h.audio_fmt == 1 && h.bits_per_sample == 8) {
    std::vector<uint8_t> tmp(num_frames * h.channels);
    size_t got = std::fread(tmp.data(), 1, tmp.size(), f);
    (void)got;
    for (size_t i = 0; i < num_frames; ++i) {
      double acc = 0;
      for (int c = 0; c < h.channels; ++c) acc += (static_cast<int>(tmp[i * h.channels + c]) - 128) / 128.0;
      mono.push_back(static_cast<float>(acc / h.channels));
    }
  } else {
    std::fclose(f);
    return Status::Error(StatusCode::UnsupportedFormat, "unsupported WAV bit depth (use 16-bit PCM)");
  }
  std::fclose(f);
  out.sample_rate = static_cast<int>(h.sample_rate);
  out.samples = std::move(mono);
  return Status::Ok();
}

Status saveWavFile(const std::string& path, const AudioBuffer& audio) {
  FILE* f = nullptr;
#if defined(_MSC_VER)
  fopen_s(&f, path.c_str(), "wb");
#else
  f = std::fopen(path.c_str(), "wb");
#endif
  if (!f) return Status::Error(StatusCode::IoError, "cannot write file: " + path);
  WavHeader h{};
  std::memcpy(h.riff, "RIFF", 4);
  std::memcpy(h.wave, "WAVE", 4);
  std::memcpy(h.fmt, "fmt ", 4);
  h.fmt_size = 16;
  h.audio_fmt = 1;
  h.channels = 1;
  h.sample_rate = static_cast<uint32_t>(audio.sample_rate);
  h.bits_per_sample = 16;
  h.block_align = 2;
  h.byte_rate = h.sample_rate * 2;
  uint32_t data_bytes = static_cast<uint32_t>(audio.samples.size() * 2);
  h.chunk_size = 36 + data_bytes;
  std::fwrite(&h, sizeof(h), 1, f);
  std::fwrite("data", 1, 4, f);
  std::fwrite(&data_bytes, 4, 1, f);
  for (float s : audio.samples) {
    float c = std::max(-1.0f, std::min(1.0f, s));
    int16_t v = static_cast<int16_t>(std::lround(c * 32767.0f));
    std::fwrite(&v, sizeof(v), 1, f);
  }
  std::fclose(f);
  return Status::Ok();
}

// --- DSP helpers -------------------------------------------------------------

AudioBuffer resampleAudio(const AudioBuffer& in, int target_rate) {
  if (in.sample_rate == target_rate || in.samples.empty()) {
    AudioBuffer o = in;
    o.sample_rate = target_rate;
    return o;
  }
  double ratio = static_cast<double>(target_rate) / in.sample_rate;
  size_t out_n = static_cast<size_t>(in.samples.size() * ratio);
  AudioBuffer out;
  out.sample_rate = target_rate;
  out.samples.resize(out_n);
  // Linear interpolation (fast + adequate for 8k->16k / 44.1k->16k).
  for (size_t i = 0; i < out_n; ++i) {
    double pos = i / ratio;
    size_t i0 = static_cast<size_t>(pos);
    size_t i1 = i0 + 1;
    double frac = pos - i0;
    float s0 = in.samples[std::min(i0, in.samples.size() - 1)];
    float s1 = in.samples[std::min(i1, in.samples.size() - 1)];
    out.samples[i] = static_cast<float>(s0 + frac * (s1 - s0));
  }
  return out;
}

AudioBuffer toMono16k(const AudioBuffer& in) { return resampleAudio(in, 16000); }

void applyPreemphasis(AudioBuffer& audio, float coeff) {
  if (audio.samples.size() < 2) return;
  for (size_t i = audio.samples.size() - 1; i > 0; --i)
    audio.samples[i] -= coeff * audio.samples[i - 1];
}

void normalizePeak(AudioBuffer& audio, float peak) {
  float m = 0;
  for (float s : audio.samples) m = std::max(m, std::fabs(s));
  if (m < 1e-7f) return;
  float g = peak / m;
  for (float& s : audio.samples) s *= g;
}

float rmsEnergy(const AudioBuffer& audio) {
  if (audio.samples.empty()) return 0.0f;
  double acc = 0;
  for (float s : audio.samples) acc += static_cast<double>(s) * s;
  return static_cast<float>(std::sqrt(acc / audio.samples.size()));
}

AudioBuffer generateSilence(double seconds, int sample_rate) {
  AudioBuffer a;
  a.sample_rate = sample_rate;
  a.samples.assign(static_cast<size_t>(seconds * sample_rate), 0.0f);
  return a;
}

AudioBuffer generateSine(double seconds, float freq_hz, float amplitude, int sample_rate) {
  AudioBuffer a;
  a.sample_rate = sample_rate;
  size_t n = static_cast<size_t>(seconds * sample_rate);
  a.samples.resize(n);
  for (size_t i = 0; i < n; ++i)
    a.samples[i] = amplitude * std::sin(2.0 * M_PI * freq_hz * i / sample_rate);
  return a;
}

AudioBuffer generateNoisySpeechLike(double seconds, int sample_rate) {
  // Deterministic amplitude-modulated harmonic complex: exercises the
  // frontend/VAD without needing real speech recordings.
  AudioBuffer a;
  a.sample_rate = sample_rate;
  size_t n = static_cast<size_t>(seconds * sample_rate);
  a.samples.resize(n);
  std::mt19937 rng(1234);
  std::normal_distribution<float> noise(0.0f, 0.02f);
  for (size_t i = 0; i < n; ++i) {
    double t = static_cast<double>(i) / sample_rate;
    double env = 0.5 + 0.5 * std::sin(2.0 * M_PI * 3.0 * t);  // 3 Hz syllabic-ish AM
    float s = static_cast<float>(env * (0.4 * std::sin(2.0 * M_PI * 120.0 * t) +
                                        0.25 * std::sin(2.0 * M_PI * 240.0 * t) +
                                        0.15 * std::sin(2.0 * M_PI * 480.0 * t)));
    a.samples[i] = s + noise(rng);
  }
  return a;
}

}  // namespace spoke
