#include "wilfred/search/vector_index.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/index/store.hpp"

namespace wilfred {

std::string default_vectors_path(const std::string& index_dir) {
  return path_join(index_dir, "vectors.bin");
}

bool VectorIndex::open(const std::string& dir, const Config& cfg) {
  dir_ = dir;
  path_ = default_vectors_path(dir);
  set_config(cfg);
  if (!enabled_) return true;
  if (file_exists(path_)) {
    if (!hnsw_.load(path_)) {
      log_warn("vectors", "vectors.bin unreadable; rebuilding");
    } else if (hnsw_.dim() != embedder_.dim()) {
      log_warn("vectors", "dimension changed; rebuilding vector index");
      hnsw_.clear();
      hnsw_.configure(embedder_.dim());
    }
  } else {
    hnsw_.configure(embedder_.dim());
  }
  return true;
}

void VectorIndex::close() {
  if (enabled_ && dirty_) save();
  dirty_ = false;
}

void VectorIndex::set_config(const Config& cfg) {
  EmbeddingConfigView ev;
  ev.enabled = cfg.embedding.enabled || cfg.providers.semantic;
  ev.backend = cfg.embedding.backend;
  ev.model = cfg.embedding.model;
  ev.endpoint = cfg.embedding.endpoint;
  ev.dim = cfg.embedding.dim;
  bool want = ev.enabled;
  // `providers.semantic` alone enables the vector sidecar with hash backend.
  if (cfg.providers.semantic && !cfg.embedding.enabled) {
    ev.enabled = true;
  }
  auto backend = embedder_.configure(ev);
  enabled_ = ev.enabled;
  min_score_ =
      (float)(cfg.embedding.enabled ? cfg.embedding.min_score : cfg.providers.semantic_min_score);
  max_results_ =
      cfg.embedding.enabled ? cfg.embedding.max_results : cfg.providers.semantic_max_results;
  if (hnsw_.dim() != 0 && hnsw_.dim() != embedder_.dim()) {
    hnsw_.clear();
  }
  if (hnsw_.dim() == 0 && enabled_) hnsw_.configure(embedder_.dim());
  (void)backend;
  (void)want;
}

bool VectorIndex::upsert(std::uint32_t id, const std::string& text) {
  if (!enabled_) return false;
  auto v = embedder_.embed(text);
  if (v.empty()) return false;
  bool ok = hnsw_.add(id, v);
  if (ok) dirty_ = true;
  return ok;
}

bool VectorIndex::remove(std::uint32_t id) {
  if (!enabled_) return false;
  bool ok = hnsw_.remove(id);
  if (ok) dirty_ = true;
  return ok;
}

bool VectorIndex::has(std::uint32_t id) const {
  if (!enabled_) return false;
  return hnsw_.get(id) != nullptr;
}

std::vector<VectorHit> VectorIndex::query(const std::string& text, int k, float min_score) const {
  std::vector<VectorHit> out;
  if (!enabled_ || hnsw_.empty()) return out;
  if (k <= 0) k = max_results_;
  auto q = embedder_.embed(text);
  if (q.empty()) return out;
  float floor = min_score >= 0 ? min_score : min_score_;
  auto hits = hnsw_.search(q, k);
  for (auto& h : hits) {
    if (h.score < floor) continue;
    out.push_back({h.id, h.score});
  }
  return out;
}

void VectorIndex::gc(const IndexStore& store) {
  if (!enabled_) return;
  // Collect stale ids (present in HNSW but dead in the store).
  std::vector<std::uint32_t> stale;
  // Probe via get(): iterate a snapshot of hits is not possible, so walk the
  // store's live set indirectly — remove anything HNSW has that store lacks.
  // HnswIndex has no iterator; use search-free approach: try all ids up to
  // next_id and drop dead ones (cheap for typical sizes, runs on checkpoint).
  for (std::uint32_t id = 1; id < store.next_id(); ++id) {
    if (hnsw_.get(id) && !store.get(id)) stale.push_back(id);
  }
  for (auto id : stale)
    hnsw_.remove(id);
  if (!stale.empty()) dirty_ = true;
}

bool VectorIndex::save() const {
  if (!enabled_) return true;
  if (!hnsw_.save(path_)) {
    log_warn("vectors", "failed to write vectors.bin");
    return false;
  }
  return true;
}

bool VectorIndex::load() {
  if (!enabled_) return true;
  if (!file_exists(path_)) {
    hnsw_.configure(embedder_.dim());
    return true;
  }
  return hnsw_.load(path_);
}

}  // namespace wilfred
