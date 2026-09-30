#pragma once

#include "wilfred/search/engine.hpp"

#include <string_view>
#include <vector>

namespace wilfred {

std::vector<SearchResult> glyph_results(std::string_view query, bool symbols);

}  // namespace wilfred
