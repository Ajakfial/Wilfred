#include "wilfred/search/context.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <ctime>

namespace wilfred {

void fill_rank_context(RankContext& ctx, const std::string& query, const Config& cfg,
                       HistoryStore* history, const std::string& clipboard) {
  ctx.query = query;
  ctx.folded = fold_search(normalize_query(query));
  ctx.tokens = tokenize_name(ctx.folded.empty() ? query : ctx.folded);
  ctx.aliases = cfg.aliases;
  ctx.context_aware = cfg.search.context_aware;
  ctx.allow_content = cfg.index.content_indexing;
  ctx.clipboard_folded = fold_search(clipboard.substr(0, 400));
  ctx.clipboard_tokens = tokenize_name(ctx.clipboard_folded);

  auto now = static_cast<std::time_t>(unix_seconds());
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  ctx.hour = local.tm_hour;
  ctx.weekday = local.tm_wday;

  if (history && history->enabled()) {
    ctx.frequency = history->freq_map();
    ctx.last_selected = history->last_map();
    ctx.learned_paths = history->choices_for(query);
    for (auto& [path, n] : history->top_selections(16)) {
      (void)n;
      auto parent = path_parent(path);
      if (!parent.empty()) ctx.recent_parents.push_back(parent);
      auto ext = path_extension(path);
      if (!ext.empty()) ctx.recent_exts.push_back(to_lower_utf8(ext));
      auto name = path_filename(path);
      if (!name.empty()) ctx.recent_names.push_back(fold_search(name));
    }
    int taken = 0;
    for (auto& q : history->recent_queries()) {
      if (q.empty() || fold_search(q) == ctx.folded) continue;
      for (auto& t : tokenize_name(fold_search(q))) {
        if (t.size() < 3) continue;
        bool dup = false;
        for (auto& e : ctx.session_tokens)
          if (e == t) {
            dup = true;
            break;
          }
        if (!dup) ctx.session_tokens.push_back(t);
      }
      if (++taken >= 8) break;
    }
  }
}

}  // namespace wilfred
