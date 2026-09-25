#include "wilfred/search/rank.hpp"

#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/fuzzy.hpp"

#include <algorithm>
#include <cmath>

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
  if (fuzzy.matched) {
    if (folded.compare(0, ctx.folded.size(), ctx.folded) == 0)
      score += w.prefix_name;
    else if (folded.find(ctx.folded) != std::string::npos)
      score += w.substring_name;
    else
      score += (w.fuzzy_name * std::max(0, fuzzy.score)) / 1000;
    if (fuzzy.score >= 600) score += w.word_boundary;
  }

  if (acronym_match(ctx.folded, name)) score += w.acronym;

  auto toks = tokenize_name(name);
  int tok_hits = 0;
  for (auto& t : ctx.tokens) {
    for (auto& n : toks)
      if (n == t || n.find(t) == 0) {
        ++tok_hits;
        break;
      }
  }
  if (!ctx.tokens.empty()) score += (w.token_proximity * tok_hits) / static_cast<int>(ctx.tokens.size());

  auto pl = fold_search(path);
  if (pl.find(ctx.folded) != std::string::npos) score += w.path_component;

  auto ext = std::string(store.pool().get(rec.ext_id));
  if (!ext.empty() && fold_search(ext).find(ctx.folded) != std::string::npos) score += w.extension;

  if (rec.kind == FileKind::Application) score += w.application;
  if (has_flag(rec.flags, RecordFlags::Directory)) score += w.directory_bonus;

  auto itf = ctx.frequency.find(path);
  if (itf != ctx.frequency.end()) {
    score += std::min(w.frequency, itf->second * 20);
  }
  auto itl = ctx.last_selected.find(path);
  if (itl != ctx.last_selected.end()) {
    auto age = unix_seconds() - itl->second;
    if (age < 86400 * 14) {
      score += w.previous_selection;
      score += static_cast<int>(w.recency * (1.0 / (1.0 + static_cast<double>(age) / 3600.0)));
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
