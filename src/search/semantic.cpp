#include "wilfred/search/semantic.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>

namespace wilfred {

namespace {

std::vector<std::uint32_t> trigrams_of(const std::string& folded) {
  std::string clean;
  clean.reserve(folded.size());
  for (unsigned char c : folded) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ')
      clean.push_back(static_cast<char>(c));
  }
  // Collapse runs of spaces so "a  b" and "a b" match.
  std::string norm;
  norm.reserve(clean.size() + 2);
  bool ws = true;
  for (char c : clean) {
    if (c == ' ') {
      if (!ws) {
        norm.push_back(' ');
        ws = true;
      }
    } else {
      norm.push_back(c);
      ws = false;
    }
  }
  while (!norm.empty() && norm.back() == ' ') norm.pop_back();
  std::string pad = "  " + norm + "  ";
  std::vector<std::uint32_t> out;
  if (pad.size() < 3) return out;
  out.reserve(pad.size() - 2);
  for (std::size_t i = 0; i + 2 < pad.size(); ++i) {
    // Skip padding artifacts so blank-heavy strings don't match everything.
    if (pad[i] == ' ' && pad[i + 1] == ' ' && pad[i + 2] == ' ') continue;
    out.push_back((static_cast<std::uint32_t>(static_cast<unsigned char>(pad[i])) << 16) |
                  (static_cast<std::uint32_t>(static_cast<unsigned char>(pad[i + 1])) << 8) |
                  static_cast<std::uint32_t>(static_cast<unsigned char>(pad[i + 2])));
  }
  std::sort(out.begin(), out.end());
  return out;
}

double cosine_sorted(const std::vector<std::uint32_t>& a, const std::vector<std::uint32_t>& b) {
  if (a.empty() || b.empty()) return 0;
  std::size_t i = 0, j = 0;
  double dot = 0;
  while (i < a.size() && j < b.size()) {
    if (a[i] == b[j]) {
      // Count multiplicities on both sides.
      std::size_t i2 = i + 1, j2 = j + 1;
      while (i2 < a.size() && a[i2] == a[i]) ++i2;
      while (j2 < b.size() && b[j2] == b[j]) ++j2;
      dot += static_cast<double>((i2 - i) * (j2 - j));
      i = i2;
      j = j2;
    } else if (a[i] < b[j]) {
      ++i;
    } else {
      ++j;
    }
  }
  if (dot <= 0) return 0;
  double na = std::sqrt(static_cast<double>(a.size()));
  double nb = std::sqrt(static_cast<double>(b.size()));
  if (na <= 0 || nb <= 0) return 0;
  double sim = dot / (na * nb);
  return sim > 1 ? 1 : sim;
}

}  // namespace

double trigram_cosine(const std::string& a, const std::string& b) {
  return cosine_sorted(trigrams_of(fold_search(a)), trigrams_of(fold_search(b)));
}

std::vector<SearchResult> SemanticProvider::query(const std::string& text, const Config& cfg,
                                                  std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.providers.semantic || limit == 0) return out;
  std::string folded = fold_search(normalize_query(text));
  int content = 0;
  for (unsigned char c : folded) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) ++content;
  }
  if (content < 3) return out;
  auto backend = to_lower_utf8(cfg.providers.semantic_backend);
  if (backend.empty()) backend = "hybrid";
  bool use_vector = backend == "vector" || backend == "hybrid";
  bool use_trigram = backend == "trigram" || backend == "hybrid";
  std::size_t budget = limit;
  if (limit > (std::size_t)cfg.providers.semantic_max_results && cfg.providers.semantic_max_results > 0)
    budget = (std::size_t)cfg.providers.semantic_max_results;
  if (budget == 0) budget = limit;

  const auto& store = index_.store();
  auto pass_filter = [&](const IndexRecord* r) {
    if (!r) return false;
    if (has_flag(r->flags, RecordFlags::System) && !cfg.search.include_system_files &&
        !cfg.search.show_system_in_results)
      return false;
    if (has_flag(r->flags, RecordFlags::Hidden) && !cfg.search.include_hidden_files) return false;
    return true;
  };
  auto push_hit = [&](std::uint32_t id, double sim, double weight, const char* tag) {
    const IndexRecord* r = store.get(id);
    if (!r || !pass_filter(r)) return;
    int score = 500 + static_cast<int>(weight * sim * 1000.0);
    for (auto& e : out)
      if (e.id == id) {
        // Merge: keep the best score, annotate hybrid source.
        if (score > e.score) e.score = score;
        return;
      }
    SearchResult sr;
    sr.id = id;
    sr.score = score;
    sr.kind = r->kind;
    sr.title = std::string(store.pool().get(r->name_id));
    sr.path = std::string(store.pool().get(r->path_id));
    auto parent = path_parent(sr.path);
    std::string label = tag;
    sr.subtitle = parent.empty() ? label : parent + " · " + label;
    sr.action = ResultAction::Open;
    sr.payload = sr.path;
    sr.kind_label = std::string(kind_name(sr.kind));
    sr.category = "semantic";
    out.push_back(std::move(sr));
  };

  // 1) Vector path: HNSW over local embeddings (llama.cpp when configured).
  if (use_vector && index_.vectors().enabled()) {
    float min_score = cfg.embedding.enabled
                          ? (float)cfg.embedding.min_score
                          : (float)cfg.providers.semantic_min_score;
    int k = (int)budget * 2;
    if (k < (int)budget) k = (int)budget;
    if (k > 50) k = 50;
    auto hits = index_.vectors().query(text, k, min_score);
    for (auto& h : hits) {
      if (out.size() >= budget) break;
      push_hit(h.id, h.score, cfg.providers.semantic_vector_weight, "semantic match");
    }
    if (!out.empty() && backend == "vector") {
      std::sort(out.begin(), out.end(),
                [](const SearchResult& a, const SearchResult& b) { return a.score > b.score; });
      if (out.size() > limit) out.resize(limit);
      return out;
    }
  }

  // 2) Trigram path: dependency-free soft match (also the fallback when the
  // vector sidecar is disabled or has no hits). Full scan, but only the top
  // `budget` hits are sorted (nth_element) so million-record stores don't pay
  // a full sort per query.
  if (use_trigram) {
    auto qtri = trigrams_of(folded);
    if (!qtri.empty()) {
      const double min_sim = cfg.providers.semantic_min_score;
      struct Hit {
        std::uint32_t id;
        double sim;
      };
      std::vector<Hit> hits;
      for (std::uint32_t i = 1; i < store.records().size(); ++i) {
        const IndexRecord* r = store.get(i);
        if (!pass_filter(r)) continue;
        std::string name(store.pool().get(r->name_id));
        std::string path(store.pool().get(r->path_id));
        double sim = cosine_sorted(qtri, trigrams_of(fold_search(name + " " + path)));
        if (sim >= min_sim) hits.push_back({i, sim});
      }
      auto cmp = [](const Hit& a, const Hit& b) { return a.sim > b.sim; };
      std::size_t keep = std::min<std::size_t>(hits.size(), budget);
      if (keep > 0) {
        std::nth_element(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(keep),
                         hits.end(), cmp);
        std::sort(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(keep), cmp);
        hits.resize(keep);
      }
      for (auto& h : hits) {
        if (out.size() >= budget) break;
        push_hit(h.id, h.sim, cfg.providers.semantic_trigram_weight,
                 backend == "hybrid" ? "soft match" : "soft match");
      }
    }
  }
  std::sort(out.begin(), out.end(),
            [](const SearchResult& a, const SearchResult& b) { return a.score > b.score; });
  if (out.size() > limit) out.resize(limit);
  return out;
}

}  // namespace wilfred
