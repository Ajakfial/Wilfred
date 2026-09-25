#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

std::vector<std::string> tokenize_name(std::string_view name);
std::vector<std::string> tokenize_content(std::string_view text, std::size_t max_tokens = 480);
bool looks_binary(std::string_view bytes);
bool content_stopword(std::string_view tok);
std::string content_snippet(std::string_view text, std::string_view query, std::size_t max_len = 120);
std::string acronym_of(std::string_view name);
std::vector<std::string> trigrams(std::string_view folded);
std::string fold_search(std::string_view s);
bool glob_match(std::string_view text, std::string_view pattern);
std::string percent_encode(std::string_view s);

}  // namespace wilfred
