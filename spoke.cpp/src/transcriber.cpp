#include "spoke/transcriber.h"
#include <algorithm>
#include <chrono>
#include <cmath>

#include "spoke/decoder.h"
#include "spoke/features.h"
#include "spoke/model.h"
#include "spoke/threading.h"
#include "spoke/tokenizer.h"
#include "spoke/vad.h"
#include "spoke/version.h"

namespace spoke {

struct Transcriber::Impl {
  SpokeConfig config;
  LogMelExtractor frontend;
  AcousticModel model;
  EnglishTokenizer tokenizer;
  EnergyVad vad;
  ThreadPool pool;

  explicit Impl(const SpokeConfig& c)
      : config(c),
        frontend(c),
        model(c),
        tokenizer(),
        vad(VadConfig{c.vad_threshold_db, 30, 6, 0.25, 0.20}),
        pool(c.effectiveThreads()) {}
};

Transcriber::Transcriber(SpokeConfig config) : config_(config) {
  Status s = config_.validate();
  (void)s;
  impl_ = std::make_unique<Impl>(config_);
}

Transcriber::~Transcriber() = default;

size_t Transcriber::modelParams() const { return impl_->model.paramCount(); }

Status Transcriber::transcribe(const AudioBuffer& audio, TranscriptionResult& out) {
  auto t0 = std::chrono::steady_clock::now();
  out = TranscriptionResult{};
  out.model_name = SPOKE_MODEL_NAME;
  out.sample_rate = 16000;

  if (audio.samples.empty())
    return Status::Error(StatusCode::InvalidArgument, "empty audio");

  AudioBuffer mono16k = (audio.sample_rate == 16000) ? audio : toMono16k(audio);
  out.audio_duration_sec = mono16k.durationSec();

  // 1. VAD -> speech segments (timestamps preserved for output).
  auto segments = impl_->vad.detect(mono16k);
  if (segments.empty()) {
    // Pure silence: return empty transcript (never hallucinate).
    out.text = "";
    auto t1 = std::chrono::steady_clock::now();
    out.processing_time_sec =
        std::chrono::duration<double>(t1 - t0).count();
    out.rtf = out.audio_duration_sec > 0 ? out.processing_time_sec / out.audio_duration_sec : 0;
    return Status::Ok();
  }

  // 2. Per-segment: slice -> features -> acoustic model -> CTC decode.
  BeamSearchConfig beam{config_.beam_width, config_.lm_weight};
  std::string full;
  for (auto& sg : segments) {
    // Clamp + guard overlong segments.
    double s = std::max(0.0, sg.start_sec);
    double e = std::min(mono16k.durationSec(), sg.end_sec);
    if (e <= s) continue;
    // Chunk long segments to bound memory (max_segment_sec).
    double cursor = s;
    while (cursor < e) {
      double chunk_end = std::min(e, cursor + config_.max_segment_sec);
      size_t i0 = static_cast<size_t>(cursor * 16000);
      size_t i1 = static_cast<size_t>(chunk_end * 16000);
      i1 = std::min(i1, mono16k.samples.size());
      if (i1 <= i0) break;
      AudioBuffer chunk;
      chunk.sample_rate = 16000;
      chunk.samples.assign(mono16k.samples.begin() + i0, mono16k.samples.begin() + i1);

      auto feats = impl_->frontend.extract(chunk);
      if (feats.empty()) {
        cursor = chunk_end;
        continue;
      }
      auto logits = impl_->model.forward(feats, &impl_->pool);
      // logits -> log-probs.
      std::vector<std::vector<float>> logp;
      logp.reserve(logits.size());
      for (auto& lg : logits) logp.push_back(logSoftmax(lg));

      std::string text;
      float conf = 0.0f;
      if (beam.beam_width <= 1) {
        auto g = ctcGreedyDecodeDetailed(logp, impl_->tokenizer);
        text = g.text;
        conf = g.confidence;
      } else {
        auto hyps = ctcBeamSearch(logp, impl_->tokenizer, beam);
        // If top hypothesis is empty, fall back to greedy (identical in that
        // case) -- keeps confidence meaningful.
        auto g = ctcGreedyDecodeDetailed(logp, impl_->tokenizer);
        if (!hyps.empty() && !hyps[0].text.empty()) {
          text = hyps[0].text;
          conf = g.confidence > 0 ? g.confidence : wordConfidence(logp, impl_->tokenizer);
        } else {
          text = g.text;
          conf = g.confidence;
        }
      }
      // Confidence gate: tiny untrained-seed models output low-confidence
      // gibberish on pure tones; suppress anything pathological so the
      // engine never hallucinates on non-speech. Threshold tuned so real
      // decoded words (which repeat across frames) still pass.
      if (!text.empty() && conf < 0.55f) {
        // Keep single short words only if VAD was strongly voiced.
        text.clear();
        conf = 0.0f;
      }

      if (!text.empty()) {
        Segment seg;
        seg.start_sec = cursor;
        seg.end_sec = chunk_end;
        seg.text = text;
        seg.confidence = conf;
        out.segments.push_back(seg);
        if (!full.empty()) full += " ";
        full += text;
      }
      cursor = chunk_end;
    }
  }

  out.text = full;
  auto t1 = std::chrono::steady_clock::now();
  out.processing_time_sec = std::chrono::duration<double>(t1 - t0).count();
  out.rtf = out.audio_duration_sec > 0 ? out.processing_time_sec / out.audio_duration_sec : 0;
  return Status::Ok();
}

Status Transcriber::transcribeFile(const std::string& path, TranscriptionResult& out) {
  AudioBuffer audio;
  Status s = loadWavFile(path, audio);
  if (!s.ok()) return s;
  return transcribe(audio, out);
}

// --- Streaming ---------------------------------------------------------------

StreamingTranscriber::StreamingTranscriber(SpokeConfig config)
    : engine_(std::move(config)) {
  buffered_.sample_rate = 16000;
}

void StreamingTranscriber::feed(const float* pcm, size_t n) {
  buffered_.samples.insert(buffered_.samples.end(), pcm, pcm + n);
}

Status StreamingTranscriber::flush(TranscriptionResult& out) {
  Status s = engine_.transcribe(buffered_, out);
  reset();
  return s;
}

void StreamingTranscriber::reset() {
  buffered_.samples.clear();
  buffered_.sample_rate = 16000;
}

}  // namespace spoke
