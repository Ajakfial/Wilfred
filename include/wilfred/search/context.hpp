#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/search/rank.hpp"

#include <string>

namespace wilfred {

void fill_rank_context(RankContext& ctx, const std::string& query, const Config& cfg,
                       HistoryStore* history, const std::string& clipboard);

}  // namespace wilfred
