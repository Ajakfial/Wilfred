#include "wilfred/search/engine.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/fuzzy.hpp"

#include <unordered_set>

namespace wilfred {

SearchEngine::SearchEngine(IndexEngine& index) : index_(index) {}

static void add_ids(std::unordered_set<std::uint32_t>& cand, const std::vector<std::uint32_t>& v,
                    std::size_t cap) {
  for (auto id : v) {
    cand.insert(id);
    if (cand.size() >= cap) return;
  }
}

std::vector<SearchResult> SearchEngine::search(const std::string& query, const Config& cfg,
                                               HistoryStore* history, std::size_t limit) {
  std::string cache_key = query + "\n" + std::to_string(limit);
  auto gen = index_.generation();
  if (gen == cache_gen_ && cache_key_ == cache_key) return cache_;

  std::string q = query;
  auto filter = parse_filter_clauses(q);
  apply_named_scopes(filter, cfg);
  q = normalize_query(q);
  const bool has_filter = !filter.extensions.empty() || !filter.kinds.empty() ||
                          !filter.in_dirs.empty() || !filter.name_contains.empty() ||
                          filter.min_size || filter.max_size || filter.min_mtime ||
                          filter.max_mtime || filter.hidden || filter.system ||
                          filter.directories_only || filter.files_only || filter.apps_only;
  if (static_cast<int>(q.size()) < cfg.search.min_query_length && !has_filter) return {};

  RankContext ctx;
  ctx.query = query;
  ctx.folded = fold_search(q);
  ctx.tokens = tokenize_name(q);
  ctx.aliases = cfg.aliases;
  if (history && history->enabled()) {
    ctx.frequency = history->freq_map();
    ctx.last_selected = history->last_map();
  }

  auto& store = index_.store();
  std::lock_guard<std::mutex> lock(store.mutex());

  std::unordered_set<std::uint32_t> cand;
  cand.reserve(4096);
  const std::size_t cap = 8000;

  for (auto& tok : ctx.tokens) {
    auto id = store.pool().find(tok);
    if (id != StringPool::kInvalid) add_ids(cand, store.posting(id), cap);
  }
  auto exact = store.pool().find(ctx.folded);
  if (exact != StringPool::kInvalid) add_ids(cand, store.posting(exact), cap);

  for (auto& tri : trigrams(ctx.folded)) {
    auto id = store.pool().find(tri);
    if (id != StringPool::kInvalid) add_ids(cand, store.trigram(id), cap);
    if (cand.size() >= cap) break;
  }

  if (!filter.extensions.empty()) {
    for (auto& e : filter.extensions) {
      auto want = fold_search(e);
      if (!want.empty() && want[0] != '.') want.insert(want.begin(), '.');
      auto eid = store.pool().find(want);
      if (eid != StringPool::kInvalid) add_ids(cand, store.ext_index().count(eid) ? store.ext_index().at(eid) : std::vector<std::uint32_t>{}, cap);
    }
  }

  if (cand.empty()) {
    std::size_t scanned = 0;
    const std::size_t scan_cap = ctx.folded.empty() ? 50000 : 20000;
    for (std::uint32_t i = 1; i < store.records().size() && scanned < scan_cap; ++i) {
      auto* r = store.get(i);
      if (!r) continue;
      ++scanned;
      if (ctx.folded.empty()) {
        cand.insert(i);
        if (cand.size() >= cap) break;
        continue;
      }
      auto name = store.pool().get(r->name_id);
      auto folded = fold_search(name);
      if (folded.find(ctx.folded) != std::string_view::npos ||
          is_subsequence(ctx.folded, folded) || acronym_match(ctx.folded, name)) {
        cand.insert(i);
        if (cand.size() >= cap) break;
      }
    }
  }

  std::vector<ScoredHit> hits;
  hits.reserve(cand.size());
  for (auto id : cand) {
    auto* r = store.get(id);
    if (!r) continue;
    if (!record_matches_filter(filter, store, *r)) continue;
    if (has_flag(r->flags, RecordFlags::System) && !cfg.search.include_system_files &&
        !cfg.search.show_system_in_results)
      continue;
    if (has_flag(r->flags, RecordFlags::Hidden) && !cfg.search.include_hidden_files) continue;
    int sc = rank_record(ctx, store, *r, cfg.ranking);
    if (sc <= 0 && !cfg.search.fuzzy) continue;
    if (sc <= 0) continue;
    hits.push_back({id, sc, r});
  }
  hits = take_top(std::move(hits), limit);

  std::vector<SearchResult> out;
  out.reserve(hits.size());
  for (auto& h : hits) {
    SearchResult sr;
    sr.id = h.id;
    sr.score = h.score;
    sr.kind = h.rec->kind;
    sr.title = std::string(store.pool().get(h.rec->name_id));
    sr.path = std::string(store.pool().get(h.rec->path_id));
    sr.subtitle = sr.path;
    sr.action = h.rec->kind == FileKind::Directory ? ResultAction::Open : ResultAction::Open;
    sr.payload = sr.path;
    out.push_back(std::move(sr));
  }
  cache_gen_ = gen;
  cache_key_ = std::move(cache_key);
  cache_ = out;
  return out;
}

}  // namespace wilfred
