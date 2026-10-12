#pragma once
#include <string>
#include "spoke/status.h"

namespace spoke {

/// Model capacity tiers. Tiny runs on anything (sub-1M params quantized).
enum class ModelSize { Tiny = 0, Base = 1, Small = 2 };

/// Global engine configuration. All modules read from this single struct so
/// the CLI, C API and MAUI layer share identical behaviour.
struct SpokeConfig {
  int sample_rate = 16000;   ///< canonical internal sample rate
  int n_fft = 400;           ///< 25 ms @ 16 kHz
  int hop_length = 160;      ///< 10 ms @ 16 kHz
  int win_length = 400;      ///< analysis window (<= n_fft)
  int n_mels = 64;           ///< mel bins (64 = sweet spot for tiny models)
  float fmin = 0.0f;
  float fmax = 8000.0f;
  bool use_cmvn = true;      ///< per-utterance cepstral mean/variance norm
  bool use_deltas = false;   ///< append delta/delta-delta (off: saves compute)

  int encoder_dim = 128;     ///< TinyEncoder hidden width
  int encoder_layers = 3;    ///< depthwise-separable conv blocks
  int num_threads = 0;       ///< 0 = auto (hardware_concurrency)
  bool use_int8 = true;      ///< quantized matmuls in encoder projection

  int beam_width = 8;        ///< CTC prefix beam width
  float lm_weight = 0.15f;   ///< shallow-fusion lexicon bonus weight
  float vad_threshold_db = -38.0f;  ///< energy VAD threshold
  int max_segment_sec = 30;  ///< split long audio into chunks

  ModelSize model_size = ModelSize::Tiny;
  std::string language = "en";

  static SpokeConfig tiny();
  static SpokeConfig base();
  Status validate() const;
  size_t effectiveThreads() const;
  std::string describe() const;
};

}  // namespace spoke
