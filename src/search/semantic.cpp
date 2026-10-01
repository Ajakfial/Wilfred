#include "wilfred/search/semantic.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cmath>
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
  for (std::size_t i = 0; i + 2 < pad.size(); ++i)
    out.push_back((static_cast<std::uint32_t>(static_cast<unsigned char>(pad[i])) << 16) |
                  (static_cast<std::uint32_t>(static_cast<unsigned char>(pad[i + 1])) << 8) |
                  static_cast<std::uint32_t>(static_cast<unsigned char>(pad[i + 2])));
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
  auto qtri = trigrams_of(folded);
  if (qtri.empty()) return out;
  const double min_sim = cfg.providers.semantic_min_score;

  const auto& store = index_.store();
  struct Hit {
    std::uint32_t id;
    double sim;
  };
  std::vector<Hit> hits;
  for (std::uint32_t i = 1; i < store.records().size(); ++i) {
    const IndexRecord* r = store.get(i);
    if (!r) continue;
    if (has_flag(r->flags, RecordFlags::System) && !cfg.search.include_system_files &&
        !cfg.search.show_system_in_results)
      continue;
    if (has_flag(r->flags, RecordFlags::Hidden) && !cfg.search.include_hidden_files) continue;
    std::string name(store.pool().get(r->name_id));
    std::string path(store.pool().get(r->path_id));
    double sim = cosine_sorted(qtri, trigrams_of(fold_search(name + " " + path)));
    if (sim >= min_sim) hits.push_back({i, sim});
  }
  std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.sim > b.sim; });
  for (auto& h : hits) {
    if (out.size() >= limit) break;
    const IndexRecord* r = store.get(h.id);
    if (!r) continue;
    SearchResult sr;
    sr.id = h.id;
    sr.score = 500 + static_cast<int>(h.sim * 1000.0);
    sr.kind = r->kind;
    sr.title = std::string(store.pool().get(r->name_id));
    sr.path = std::string(store.pool().get(r->path_id));
    auto parent = path_parent(sr.path);
    sr.subtitle = parent.empty() ? "soft match" : parent + " · soft match";
    sr.action = ResultAction::Open;
    sr.payload = sr.path;
    sr.kind_label = std::string(kind_name(sr.kind));
    sr.category = "semantic";
    out.push_back(std::move(sr));
  }
  return out;
}

}  // namespace wilfred
