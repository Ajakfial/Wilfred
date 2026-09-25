#pragma once

#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

std::string overlay_ui_dir();
std::string overlay_json_escape(const std::string& s);
const char* overlay_action_name(ResultAction a);
std::string overlay_results_json(const std::vector<SearchResult>& items);
bool overlay_json_field(const std::string& json, const char* key, std::string& out);

}  // namespace wilfred
