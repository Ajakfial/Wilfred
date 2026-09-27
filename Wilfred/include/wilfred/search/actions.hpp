#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

class PluginHost;
void set_plugin_host_for_actions(PluginHost* host);

void attach_result_actions(SearchResult& r);
void attach_result_actions(std::vector<SearchResult>& results);
bool execute_result_action(const SearchResult& r, const Config& cfg, const std::string& action_id);
bool action_hides_overlay(const std::string& action_id);
bool native_simulate_paste();

}  // namespace wilfred
