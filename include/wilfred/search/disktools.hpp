#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/index/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct SearchResult;

// Top-N largest files from the index, optionally under dir_substr.
std::vector<SearchResult> large_file_results(IndexEngine& index, const Config& cfg,
                                             const std::string& dir_filter, int limit = 8);
// Duplicate candidates grouped by size then verified by hash (first groups only).
std::vector<SearchResult> dupe_file_results(IndexEngine& index, const Config& cfg,
                                            const std::string& dir_filter, int limit = 8);

}  // namespace wilfred
