#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wilfred {

std::unordered_map<std::string, std::string> builtin_macros();
std::unordered_map<std::string, std::string> merged_macros(const Config& cfg);

struct MacroMatch {
  std::string name;
  std::string tmpl;
  std::string argument;
  bool matched{false};
};

MacroMatch match_macro(std::string_view query, const Config& cfg);
std::string expand_macro(const std::string& tmpl, const std::string& argument,
                         const std::string& clipboard);
std::vector<SearchResult> macro_results(const MacroMatch& m, const std::string& clipboard);

}  // namespace wilfred
