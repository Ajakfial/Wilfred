#pragma once
#include <memory>
#include <string>
#include <vector>
#include "spoke/audio.h"
#include "spoke/config.h"
#include "spoke/status.h"

namespace spoke {

struct Segment {
  double start_sec = 0.0;
  double end_sec = 0.0;
  std::string text;
  float confidence = 0.0f;
};

struct TranscriptionResult {
  std::string text;
  std::vector<Segment> segments;
  double processing_time_sec = 0.0;
  double audio_duration_sec = 0.0;
  double rtf = 0.0;  // processing_time / audio_duration (lower is better)
  std::string model_name = "spoke-tiny.en";
  int sample_rate = 16000;
};

/// High-level transcription engine. Owns config, frontend, model, decoder.
/// Thread-safe for concurrent transcribe() calls (model is stateless).
class Transcriber {
 public:
  explicit Transcriber(SpokeConfig config = SpokeConfig::tiny());
  ~Transcriber();

  const SpokeConfig& config() const { return config_; }

  Status transcribe(const AudioBuffer& audio, TranscriptionResult& out);
  Status transcribeFile(const std::string& path, TranscriptionResult& out);

  size_t modelParams() const;

 private:
  SpokeConfig config_;
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Incremental/streaming wrapper: feed 16 kHz PCM, flush at utterance end.
class StreamingTranscriber {
 public:
  explicit StreamingTranscriber(SpokeConfig config = SpokeConfig::tiny());
  void feed(const float* pcm, size_t n);
  void feed(const std::vector<float>& pcm) { feed(pcm.data(), pcm.size()); }
  Status flush(TranscriptionResult& out);
  void reset();

 private:
  Transcriber engine_;
  AudioBuffer buffered_;
};

}  // namespace spoke
