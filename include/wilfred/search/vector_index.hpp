#pragma once

// Persistent vector sidecar for semantic search.
//
// Layout (next to the WAL/snapshot in the index directory):
//   vectors.bin  — HNSW graph + float vectors keyed by IndexRecord id
// ("vectors.wal" is reserved for future incremental journaling; v1 reuses the
// main snapshot checkpoint cadence so vectors never drift far from records.)
//
// Vectors are produced by LocalEmbedder (llama.cpp server/model when
// configured, otherwise the built-in hash embedder) over
// `name + " " + path [+ content tokens]` and queried with cosine similarity.

#include "wilfred/search/embedding.hpp"
#include "wilfred/search/hnsw.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace wilfred {

class IndexStore;
struct Config;

struct VectorHit {
  std::uint32_t id{0};
  float score{0};
};

class VectorIndex {
public:
  VectorIndex() = default;

  bool open(const std::string& dir, const Config& cfg);
  void close();

  bool enabled() const { return enabled_; }
  int dim() const { return embedder_.dim(); }
  std::size_t size() const { return hnsw_.size(); }
  bool has(std::uint32_t id) const;
  std::string backend() const { return embedder_.backend(); }

  // (Re)build the vector for one record. `text` should be name+path+content.
  bool upsert(std::uint32_t id, const std::string& text);
  bool remove(std::uint32_t id);
  void set_config(const Config& cfg);

  std::vector<VectorHit> query(const std::string& text, int k, float min_score) const;

  // Drop vectors whose ids no longer exist in the store.
  void gc(const IndexStore& store);
  bool save() const;
  bool load();

private:
  std::string dir_;
  std::string path_;
  bool enabled_{false};
  float min_score_{0.45f};
  int max_results_{10};
  LocalEmbedder embedder_;
  HnswIndex hnsw_;
  bool dirty_{false};
};

std::string default_vectors_path(const std::string& index_dir);

}  // namespace wilfred
