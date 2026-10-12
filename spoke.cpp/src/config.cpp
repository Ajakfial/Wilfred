#include "spoke/config.h"
#include <thread>

namespace spoke {

SpokeConfig SpokeConfig::tiny() {
  SpokeConfig c;
  c.sample_rate = 16000;
  c.n_fft = 400;
  c.hop_length = 160;
  c.win_length = 400;
  c.n_mels = 64;
  c.encoder_dim = 128;
  c.encoder_layers = 3;
  c.model_size = ModelSize::Tiny;
  c.beam_width = 8;
  return c;
}

SpokeConfig SpokeConfig::base() {
  SpokeConfig c = tiny();
  c.encoder_dim = 192;
  c.encoder_layers = 5;
  c.n_mels = 80;
  c.model_size = ModelSize::Base;
  c.beam_width = 10;
  return c;
}

Status SpokeConfig::validate() const {
  if (sample_rate <= 0) return Status::Error(StatusCode::InvalidArgument, "sample_rate must be > 0");
  if (n_fft <= 0 || hop_length <= 0) return Status::Error(StatusCode::InvalidArgument, "n_fft/hop must be > 0");
  if (win_length > n_fft) return Status::Error(StatusCode::InvalidArgument, "win_length <= n_fft required");
  if (n_mels <= 0 || n_mels > 128) return Status::Error(StatusCode::InvalidArgument, "n_mels out of range");
  if (encoder_dim <= 0 || encoder_layers <= 0) return Status::Error(StatusCode::InvalidArgument, "encoder dims invalid");
  if (beam_width <= 0 || beam_width > 64) return Status::Error(StatusCode::InvalidArgument, "beam_width out of range");
  return Status::Ok();
}

size_t SpokeConfig::effectiveThreads() const {
  if (num_threads > 0) return static_cast<size_t>(num_threads);
  unsigned n = std::thread::hardware_concurrency();
  if (n == 0) n = 4;
  if (n > 8) n = 8;
  return n;
}

std::string SpokeConfig::describe() const {
  std::string s = (model_size == ModelSize::Tiny ? "tiny" : model_size == ModelSize::Base ? "base" : "small");
  s += " dim=" + std::to_string(encoder_dim) + " layers=" + std::to_string(encoder_layers);
  s += " mels=" + std::to_string(n_mels) + " threads=" + std::to_string(effectiveThreads());
  return s;
}

}  // namespace spoke
