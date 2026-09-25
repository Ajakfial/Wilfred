#pragma once

// Internal UI helpers shared by platform overlays.
#include "wilfred/search/engine.hpp"

#include <functional>
#include <string>
#include <vector>

namespace wilfred {

using OverlaySubmit = std::function<void(const SearchResult&)>;
using OverlayQuery = std::function<std::vector<SearchResult>(const std::string&)>;

void overlay_bind(OverlayQuery q, OverlaySubmit s);
void overlay_set_quit(std::function<void()> fn);
void overlay_pump();

}  // namespace wilfred
