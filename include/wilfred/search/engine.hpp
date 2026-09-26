#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/search/filter.hpp"
#include "wilfred/search/rank.hpp"

#include <string>
#include <vector>

namespace wilfred {

enum class ResultAction { Open, Reveal, Copy, WebSearch, Calculate, Convert, None, Habit, Mini, Expand, Plugin };

struct ResultActionItem {
  std::string id;
  std::string label;
};

struct SearchResult {
  std::uint32_t id{0};
  int score{0};
  FileKind kind{FileKind::Unknown};
  std::string title;
  std::string subtitle;
  std::string path;
  ResultAction action{ResultAction::Open};
  std::string payload;
  std::string kind_label;
  std::string category;
  int meter{-1};
  std::string plugin_id;
  std::vector<ResultActionItem> actions;
};

class SearchEngine {
public:
  explicit SearchEngine(IndexEngine& index);

  std::vector<SearchResult> search(const std::string& query, const Config& cfg,
                                   HistoryStore* history, std::size_t limit);

private:
  IndexEngine& index_;
  std::uint64_t cache_gen_{0};
  std::string cache_key_;
  std::vector<SearchResult> cache_;
};

}  // namespace wilfred
