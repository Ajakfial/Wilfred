#pragma once

// Local embedding abstraction.
//
// Wilfred ships a dependency-free hashed embedder that works everywhere with
// zero downloads. When the user configures `embedding.model` (a local .gguf
// file for use with llama.cpp) or `embedding.endpoint` (a local llama.cpp
// server started with `--embedding`, e.g. `llama-server -m model.gguf
// --embedding`), the embedder prefers that backend and falls back to the
// hashed embedder when the server/model is unreachable.
//
// The llama.cpp integration is intentionally runtime-only (HTTP to a local
// server, or dlopen when a libllama build is present) so Wilfred stays a
// single small binary with no link-time dependency on llama.cpp.

#include <cstddef>
#include <string>
#include <vector>

namespace wilfred {

struct EmbeddingConfigView {
  bool enabled{false};
  std::string backend{"auto"};  // auto | hash | llamacpp | server
  std::string model;
  std::string endpoint;
  int dim{384};
};

// Deterministic, dependency-free text embedding. Char 3-grams + word hashes
// are projected into `dim` buckets and L2-normalized, so cosine similarity is
// a dot product. Good enough for on-device fuzzy/semantic ranking and always
// available offline.
std::vector<float> hash_embed_text(const std::string& text, int dim);

// Cosine similarity in [-1,1] for L2-normalized (or arbitrary) vectors.
float embedding_cosine(const std::vector<float>& a, const std::vector<float>& b);

// Local embedder: tries llama.cpp first (server endpoint, then model file via
// optional runtime-loaded libllama), falls back to hash_embed_text.
class LocalEmbedder {
 public:
  LocalEmbedder() = default;

  // Configure from Config. Returns the active backend name for logging.
  std::string configure(const EmbeddingConfigView& cfg);

  bool available() const { return dim_ > 0; }
  int dim() const { return dim_; }
  std::string backend() const { return active_backend_; }

  // Embed text into a dim-dimensional L2-normalized vector.
  std::vector<float> embed(const std::string& text) const;

  // True when a llama.cpp backend (server or model file) is configured.
  bool wants_llamacpp() const;

  // Probe a llama.cpp embedding server (GET /health or POST /embedding).
  // Never throws; returns false on any failure.
  static bool probe_server(const std::string& endpoint, int timeout_ms = 1500);

 private:
  std::vector<float> embed_via_server(const std::string& text) const;
  std::vector<float> embed_via_llamacpp(const std::string& text) const;

  EmbeddingConfigView cfg_;
  std::string active_backend_{"hash"};
  int dim_{384};
};

}  // namespace wilfred
