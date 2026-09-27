#include "wilfred/search/engine.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/content.hpp"
#include "wilfred/search/context.hpp"
#include "wilfred/search/fuzzy.hpp"

#include <algorithm>
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

static SearchResult result_from_record(const IndexStore& store, const ScoredHit& h, bool content_hit) {
  SearchResult sr;
  sr.id = h.id;
  sr.score = h.score;
  sr.kind = h.rec->kind;
  sr.title = std::string(store.pool().get(h.rec->name_id));
  sr.path = std::string(store.pool().get(h.rec->path_id));
  sr.subtitle = path_parent(sr.path);
  if (sr.subtitle.empty()) sr.subtitle = sr.path;
  if (content_hit) {
    sr.subtitle = "In file contents · " + sr.subtitle;
    sr.category = "content";
    sr.kind_label = "content";
  }
  sr.action = ResultAction::Open;
  sr.payload = sr.path;
  sr.kind_label = std::string(kind_name(sr.kind));
  return sr;
}

static bool tokens_in_text(const std::vector<std::string>& tokens, std::string_view folded_name,
                           std::string_view folded_path) {
  for (auto& t : tokens) {
    if (t.size() < 2) continue;
    if (folded_name.find(t) == std::string_view::npos && folded_path.find(t) == std::string_view::npos &&
        !is_subsequence(t, folded_name))
      return false;
  }
  return true;
}

std::vector<SearchResult> SearchEngine::search(const std::string& query, const Config& cfg,
                                               HistoryStore* history, std::size_t limit) {
  ClipboardSnapshot clip;
  if (cfg.search.clipboard) clip = read_clipboard();

  std::string cache_key = query + "\n" + std::to_string(limit) + "\n" +
                          std::to_string(history && history->enabled() ? history->generation() : 0) +
                          "\n" + clip.text.substr(0, 64);
  auto gen = index_.generation();
  if (gen == cache_gen_ && cache_key_ == cache_key) return cache_;

  std::string q = query;
  auto filter = parse_filter_clauses(q);
  apply_named_scopes(filter, cfg);
  q = normalize_query(q);
  const bool has_filter = !filter.extensions.empty() || !filter.kinds.empty() ||
                          !filter.in_dirs.empty() || !filter.name_contains.empty() ||
                          !filter.phrases.empty() || !filter.content_contains.empty() ||
                          filter.min_size || filter.max_size || filter.min_mtime ||
                          filter.max_mtime || filter.hidden || filter.system ||
                          filter.directories_only || filter.files_only || filter.apps_only ||
                          filter.content_only;
  if (static_cast<int>(q.size()) < cfg.search.min_query_length && !has_filter) {
    cache_gen_ = gen;
    cache_key_ = std::move(cache_key);
    cache_.clear();
    return cache_;
  }

  // Use the filter-stripped query text (q) for ranking context, not the raw
  // input. When a query is made up entirely of filter clauses (e.g. "*.cpp in
  // Projects"), q is legitimately empty here even though the original query
  // string was not; falling back to the raw query would leak filter syntax
  // ("in", "*.cpp", ...) into the free-text ranker, which then requires
  // filenames to fuzzy/substring match that filter syntax and rejects every
  // otherwise-matching, filtered result. An empty q is handled by
  // fill_rank_context/rank_record as a filter-only search.
  RankContext ctx;
  fill_rank_context(ctx, q, cfg, history, cfg.search.clipboard ? clip.text : "");
  ctx.clipboard_paths = clipboard_path_hints(clip);
  if (!cfg.search.context_aware) {
    ctx.recent_parents.clear();
    ctx.recent_exts.clear();
    ctx.recent_names.clear();
  }

  auto& store = index_.store();
  std::lock_guard<std::recursive_mutex> lock(store.mutex());

  std::unordered_set<std::uint32_t> cand;
  cand.reserve(4096);
  const std::size_t cap = 8000;

  auto consider_tokens = ctx.tokens;
  for (auto& t : filter.content_contains) consider_tokens.push_back(t);

  for (auto& tok : consider_tokens) {
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
      if (eid != StringPool::kInvalid)
        add_ids(cand, store.ext_index().count(eid) ? store.ext_index().at(eid)
                                                   : std::vector<std::uint32_t>{},
                cap);
    }
  }

  for (auto& hint : ctx.clipboard_paths) {
    if (auto* r = store.by_path(hint)) cand.insert(r->id);
  }
  if (cfg.search.clipboard) {
    for (auto& tok : ctx.clipboard_tokens) {
      if (tok.size() < 4) continue;
      auto id = store.pool().find(tok);
      if (id != StringPool::kInvalid) add_ids(cand, store.posting(id), cap);
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
          is_subsequence(ctx.folded, folded) || acronym_match(ctx.folded, name) ||
          store.content_token_hits(i, ctx.tokens) > 0) {
        cand.insert(i);
        if (cand.size() >= cap) break;
      }
    }
  }

  std::vector<ScoredHit> hits;
  hits.reserve(cand.size());
  const bool strict_tokens = ctx.tokens.size() >= 2 && !filter.content_only;
  int best = 0;
  std::unordered_set<std::uint32_t> content_ids;
  for (auto id : cand) {
    auto* r = store.get(id);
    if (!r) continue;
    if (!record_matches_filter(filter, store, *r)) continue;
    if (has_flag(r->flags, RecordFlags::System) && !cfg.search.include_system_files &&
        !cfg.search.show_system_in_results)
      continue;
    if (has_flag(r->flags, RecordFlags::Hidden) && !cfg.search.include_hidden_files) continue;
    auto name = store.pool().get(r->name_id);
    auto pathv = store.pool().get(r->path_id);
    int content_hits = 0;
    if (ctx.allow_content) content_hits = store.content_token_hits(id, ctx.tokens);
    if (!filter.content_contains.empty())
      content_hits = std::max(content_hits, store.content_token_hits(id, filter.content_contains));
    if (strict_tokens) {
      auto nf = fold_search(name);
      auto pf = fold_search(pathv);
      if (!tokens_in_text(ctx.tokens, nf, pf) && content_hits <= 0) continue;
    }
    int sc = rank_record(ctx, store, *r, cfg.ranking);
    if (sc <= 0) continue;
    if (!cfg.search.fuzzy && sc < cfg.ranking.substring_name && content_hits <= 0) continue;
    if (content_hits > 0) content_ids.insert(id);
    if (sc > best) best = sc;
    hits.push_back({id, sc, r});
  }
  if (best > 0 && ctx.folded.size() >= 2 && !hits.empty()) {
    int pct = ctx.folded.size() >= 4 ? 52 : 42;
    if (best >= cfg.ranking.exact_name) pct = std::max(pct, 58);
    int floor = std::max(1, (best * pct) / 100);
    hits.erase(std::remove_if(hits.begin(), hits.end(),
                              [floor](const ScoredHit& h) { return h.score < floor; }),
               hits.end());
  }
  hits = take_top(std::move(hits), limit);

  std::vector<SearchResult> out;
  out.reserve(hits.size() + 8);

  if (cfg.search.clipboard) {
    if (!clip.text.empty() &&
        (q.empty() || clipboard_text_matches(q.empty() ? query : q, clip.text))) {
      SearchResult cr;
      cr.title = clipboard_preview(clip.text);
      auto snip = content_snippet(clip.text, q.empty() ? query : q, 88);
      cr.subtitle = snip.empty() ? "Clipboard" : "Clipboard · " + snip;
      cr.payload = clip.text;
      cr.path = clip.text;
      cr.action = ResultAction::Copy;
      cr.score = q.empty() ? 8600 : 9400;
      cr.kind_label = "clipboard";
      cr.category = "clipboard";
      out.push_back(std::move(cr));
    }
    for (auto& hint : ctx.clipboard_paths) {
      if (hint.empty()) continue;
      auto folded_hint = fold_search(path_filename(hint));
      if (!ctx.folded.empty() && folded_hint.find(ctx.folded) == std::string::npos &&
          fold_search(hint).find(ctx.folded) == std::string::npos)
        continue;
      if (store.by_path(hint)) continue;
      if (!file_exists(hint)) continue;
      SearchResult pr;
      pr.title = path_filename(hint);
      if (pr.title.empty()) pr.title = hint;
      pr.subtitle = "From clipboard · " + path_parent(hint);
      pr.path = hint;
      pr.payload = hint;
      pr.action = ResultAction::Open;
      pr.score = 8800;
      pr.kind_label = "clipboard";
      pr.category = "clipboard";
      out.push_back(std::move(pr));
    }
  }

  for (auto& h : hits) {
    out.push_back(result_from_record(store, h, content_ids.count(h.id) != 0));
  }

  cache_gen_ = gen;
  cache_key_ = std::move(cache_key);
  cache_ = out;
  return out;
}

}  // namespace wilfred
