#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

// `plugins` overlay mini: installed list + trust-on-first-use approvals.
// Payloads are `plugin_approve:<id>` / `plugin_approve_all`, executed in
// execute_result_action (approve + write trust store).
std::vector<SearchResult> plugin_results(const std::string& remainder, const Config& cfg);

}  // namespace wilfred
