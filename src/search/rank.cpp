#include "wilfred/search/rank.hpp"

#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/fuzzy.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>

namespace wilfred {

int rank_record(const RankContext& ctx, const IndexStore& store, const IndexRecord& rec,
                const RankingWeights& w) {
  auto name = std::string(store.pool().get(rec.name_id));
  auto path = std::string(store.pool().get(rec.path_id));
  auto folded = fold_search(name);
  int score = 0;

  if (ctx.folded.empty()) {
    if (rec.kind == FileKind::Application) score += w.application;
    if (has_flag(rec.flags, RecordFlags::Directory)) score += w.directory_bonus;
    auto itf = ctx.frequency.find(path);
    if (itf != ctx.frequency.end()) score += std::min(w.frequency, itf->second * 20);
    return std::max(1, score);
  }

  if (folded == ctx.folded) score += w.exact_name;
  auto fuzzy = score_fuzzy(ctx.folded, folded, name);
  const bool name_hit = fuzzy.matched;
  if (name_hit) {
    if (folded.compare(0, ctx.folded.size(), ctx.folded) == 0)
      score += w.prefix_name;
    else if (folded.find(ctx.folded) != std::string::npos)
      score += w.substring_name;
    else
      score += (w.fuzzy_name * std::max(0, fuzzy.score)) / 1000;
    if (fuzzy.score >= 600) score += w.word_boundary;
  }

  const bool acro = acronym_match(ctx.folded, name);
  if (acro) score += w.acronym;

  auto toks = tokenize_name(name);
  int tok_hits = 0;
  for (auto& t : ctx.tokens) {
    for (auto& n : toks)
      if (n == t || n.find(t) == 0) {
        ++tok_hits;
        break;
      }
  }
  if (!ctx.tokens.empty()) {
    score += (w.token_proximity * tok_hits) / static_cast<int>(ctx.tokens.size());
    int miss = static_cast<int>(ctx.tokens.size()) - tok_hits;
    if (miss > 0) score -= (w.token_proximity * miss) / 2;
  }

  auto pl = fold_search(path);
  const bool path_hit = pl.find(ctx.folded) != std::string::npos;
  if (path_hit) {
    int path_score = w.path_component;
    if (ctx.folded.size() <= 2) path_score /= 3;
    if (!name_hit && !acro) path_score /= 2;
    score += path_score;
  }

  int content_hits = 0;
  if (ctx.allow_content && !ctx.tokens.empty())
    content_hits = store.content_token_hits(rec.id, ctx.tokens);
  if (content_hits > 0) {
    score += (w.content_hit * content_hits) / std::max(1, static_cast<int>(ctx.tokens.size()));
    if (!name_hit && !acro) score += w.content_hit / 3;
  }

  if (!name_hit && !acro && !path_hit && content_hits <= 0) return 0;

  auto ext = std::string(store.pool().get(rec.ext_id));
  if (!ext.empty() && fold_search(ext).find(ctx.folded) != std::string::npos) score += w.extension;

  if (rec.kind == FileKind::Application) score += w.application;
  if (has_flag(rec.flags, RecordFlags::Directory)) score += w.directory_bonus;

  auto itf = ctx.frequency.find(path);
  if (itf != ctx.frequency.end()) {
    score += std::min(w.frequency, itf->second * 20);
  }
  auto itlearn = ctx.learned_paths.find(path);
  if (itlearn != ctx.learned_paths.end()) {
    score += std::min(w.learned_choice, itlearn->second * 90);
  }
  auto itl = ctx.last_selected.find(path);
  if (itl != ctx.last_selected.end()) {
    auto age = unix_seconds() - itl->second;
    if (age < 86400 * 14) {
      score += w.previous_selection;
      score += static_cast<int>(w.recency * (1.0 / (1.0 + static_cast<double>(age) / 3600.0)));
      if (ctx.context_aware && w.hour_affinity > 0) {
        auto when = static_cast<std::time_t>(itl->second);
        std::tm local{};
#ifdef _WIN32
        localtime_s(&local, &when);
#else
        localtime_r(&when, &local);
#endif
        int dh = std::abs(local.tm_hour - ctx.hour);
        if (dh > 12) dh = 24 - dh;
        if (dh <= 2) score += w.hour_affinity;
        if (local.tm_wday == ctx.weekday) score += w.hour_affinity / 2;
      }
    }
  }

  if (ctx.context_aware) {
    auto parent = path;
    auto slash = parent.find_last_of("/\\");
    if (slash != std::string::npos) parent.resize(slash);
    for (auto& rp : ctx.recent_parents) {
      if (!rp.empty() && (parent == rp || path.find(rp) != std::string::npos)) {
        score += w.context_parent;
        break;
      }
    }
    auto extl = to_lower_utf8(ext);
    for (auto& re : ctx.recent_exts) {
      if (!re.empty() && extl == re) {
        score += w.context_extension;
        break;
      }
    }
    for (auto& rn : ctx.recent_names) {
      if (!rn.empty() && (folded == rn || folded.find(rn) != std::string::npos)) {
        score += w.context_parent / 2;
        break;
      }
    }
    if (rec.atime > 0) {
      auto age = unix_seconds() - rec.atime;
      if (age >= 0 && age < 86400 * 3)
        score += static_cast<int>(w.access_recency * (1.0 / (1.0 + static_cast<double>(age) / 7200.0)));
    }
    if (rec.kind == FileKind::Application && (ctx.hour >= 18 || ctx.hour < 8))
      score += w.hour_affinity / 2;
    if ((rec.kind == FileKind::Source || rec.kind == FileKind::Document) && ctx.hour >= 9 &&
        ctx.hour <= 18)
      score += w.hour_affinity / 3;
    if (!ctx.session_tokens.empty()) {
      int session_hits = 0;
      for (auto& t : ctx.session_tokens) {
        if (folded.find(t) != std::string::npos || pl.find(t) != std::string::npos) ++session_hits;
      }
      if (session_hits)
        score += (w.context_parent * std::min(session_hits, 4)) / 5;
    }
  }

  if (!ctx.clipboard_folded.empty() && ctx.clipboard_folded.size() >= 3) {
    if (folded.find(ctx.clipboard_folded) != std::string::npos ||
        ctx.clipboard_folded.find(folded) != std::string::npos)
      score += w.clipboard_overlap;
    else if (pl.find(ctx.clipboard_folded) != std::string::npos)
      score += w.clipboard_overlap / 2;
  }
  if (!ctx.clipboard_tokens.empty()) {
    int hits = 0;
    for (auto& t : ctx.clipboard_tokens) {
      if (t.size() < 4) continue;
      if (folded.find(t) != std::string::npos || pl.find(t) != std::string::npos) ++hits;
    }
    if (hits) score += (w.clipboard_overlap * std::min(hits, 3)) / 4;
  }
  for (auto& hint : ctx.clipboard_paths) {
    if (!hint.empty() && (path == hint || path.find(hint) != std::string::npos ||
                          hint.find(path) != std::string::npos)) {
      score += w.clipboard_overlap;
      break;
    }
  }

  for (auto& [alias, target] : ctx.aliases) {
    if (alias == ctx.folded) {
      auto tn = fold_search(target);
      if (tn == folded || folded.find(tn) != std::string::npos || tn.find(folded) != std::string::npos)
        score += w.alias;
    }
  }

  // Prefer shorter names slightly when already matching.
  if (fuzzy.matched) score += std::max(0, 80 - static_cast<int>(name.size()));
  score += default_rank_pipeline().extras(ctx, store, rec, w);
  return score;
}

void RankPipeline::add(std::string name, Signal signal) {
  names_.push_back(std::move(name));
  signals_.push_back(std::move(signal));
}

int RankPipeline::extras(const RankContext& ctx, const IndexStore& store, const IndexRecord& rec,
                         const RankingWeights& w) const {
  int s = 0;
  for (auto& fn : signals_) {
    if (fn) s += fn(ctx, store, rec, w);
  }
  return s;
}

void RankPipeline::clear() {
  names_.clear();
  signals_.clear();
}

RankPipeline& default_rank_pipeline() {
  static RankPipeline p;
  return p;
}

std::vector<ScoredHit> take_top(std::vector<ScoredHit> hits, std::size_t n) {
  if (hits.size() <= n) {
    std::sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.score > b.score; });
    return hits;
  }
  std::partial_sort(hits.begin(), hits.begin() + static_cast<std::ptrdiff_t>(n), hits.end(),
                    [](auto& a, auto& b) { return a.score > b.score; });
  hits.resize(n);
  return hits;
}

}  // namespace wilfred
