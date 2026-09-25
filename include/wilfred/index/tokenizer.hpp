#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

std::vector<std::string> tokenize_name(std::string_view name);
std::string acronym_of(std::string_view name);
std::vector<std::string> trigrams(std::string_view folded);
std::string fold_search(std::string_view s);
bool glob_match(std::string_view text, std::string_view pattern);

}  // namespace wilfred
