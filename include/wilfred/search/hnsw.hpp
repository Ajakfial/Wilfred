#pragma once

// Dependency-free HNSW (Hierarchical Navigable Small World) vector index.
//
// Stores one float vector per index record id alongside the existing
// snapshot/WAL files (`vectors.bin` next to `snapshot.wilf` + `journal.wal`).
// The implementation favors small code + cross-platform determinism over
// absolute recall: greedy upper-layer descent + ef-limited base-layer search,
// cosine similarity via dot product of L2-normalized vectors.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace wilfred {

struct HnswHit {
  std::uint32_t id{0};
  float score{0};  // cosine in [-1,1]
};

class HnswIndex {
public:
  HnswIndex() = default;
  explicit HnswIndex(int dim, int m = 16, int ef_construction = 64);

  int dim() const { return dim_; }
  std::size_t size() const { return ids_.size(); }
  bool empty() const { return ids_.empty(); }

  void configure(int dim, int m = 16, int ef_construction = 64);
  void clear();

  // Insert or replace the vector for `id`. Returns false on dim mismatch.
  bool add(std::uint32_t id, const std::vector<float>& vec);
  bool remove(std::uint32_t id);
  bool has(std::uint32_t id) const;

  // k-nearest by cosine. Returns up to k hits sorted by score desc.
  std::vector<HnswHit> search(const std::vector<float>& query, int k, int ef = 32) const;

  const std::vector<float>* get(std::uint32_t id) const;

  bool save(const std::string& path) const;
  bool load(const std::string& path);

private:
  int level_of(std::uint64_t counter) const;
  float dist2(std::size_t a, std::size_t b) const;
  static float dot(const std::vector<float>& a, const std::vector<float>& b);

  int dim_{0};
  int m_{16};
  int ef_construction_{64};
  std::uint64_t counter_{0};
  std::uint32_t entry_{0};  // internal position of entry point

  std::vector<std::uint32_t> ids_;        // pos -> record id
  std::vector<std::vector<float>> vecs_;  // pos -> vector
  std::vector<int> levels_;               // pos -> level
  // links[pos][level] = neighbor positions
  std::vector<std::vector<std::vector<std::uint32_t>>> links_;
};

}  // namespace wilfred
