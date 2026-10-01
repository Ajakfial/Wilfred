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

// Typo-tolerant helpers shared by fuzzy matching, command correction,
// autocomplete and ranking. All are side-effect-free and cheap for
// per-keystroke use (bounded, early-exit, ASCII-folded inputs).
int typo_threshold(std::size_t len);
int damerau_bounded(std::string_view a, std::string_view b, int max_dist);
bool is_typo_match(std::string_view query, std::string_view candidate);
// True when query is a typo-prefix of candidate's head (same length window).
bool is_typo_prefix(std::string_view query, std::string_view candidate);
// Typo score 0..1000 for display/ranking (0 = no typo relation).
int typo_score(std::string_view query_folded, std::string_view text_folded);

}  // namespace wilfred
