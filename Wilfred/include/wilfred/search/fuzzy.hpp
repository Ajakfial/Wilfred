#pragma once

#include <string>
#include <string_view>

namespace wilfred {

struct FuzzyScore {
  int score{0};
  bool matched{false};
};

FuzzyScore score_fuzzy(std::string_view query_folded, std::string_view text_folded,
                       std::string_view text_original);
bool is_subsequence(std::string_view query, std::string_view text);
bool acronym_match(std::string_view query, std::string_view text);
int levenshtein_bounded(std::string_view a, std::string_view b, int max_dist);

}  // namespace wilfred
