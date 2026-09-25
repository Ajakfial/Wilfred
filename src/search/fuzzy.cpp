#include "wilfred/search/fuzzy.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <vector>

namespace wilfred {

bool is_subsequence(std::string_view query, std::string_view text) {
  std::size_t j = 0;
  for (std::size_t i = 0; i < text.size() && j < query.size(); ++i) {
    if (text[i] == query[j]) ++j;
  }
  return j == query.size();
}

bool acronym_match(std::string_view query, std::string_view text) {
  auto ac = acronym_of(text);
  if (ac.empty()) return false;
  auto q = fold_search(query);
  return ac.find(q) != std::string::npos || q.find(ac) != std::string::npos || ac == q;
}

int levenshtein_bounded(std::string_view a, std::string_view b, int max_dist) {
  if (a == b) return 0;
  if (static_cast<int>(std::abs(static_cast<int>(a.size()) - static_cast<int>(b.size()))) >
      max_dist)
    return max_dist + 1;
  std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = static_cast<int>(j);
  for (std::size_t i = 1; i <= a.size(); ++i) {
    cur[0] = static_cast<int>(i);
    int row_min = cur[0];
    for (std::size_t j = 1; j <= b.size(); ++j) {
      int cost = a[i - 1] == b[j - 1] ? 0 : 1;
      cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
      row_min = std::min(row_min, cur[j]);
    }
    if (row_min > max_dist) return max_dist + 1;
    prev.swap(cur);
  }
  return prev[b.size()];
}

FuzzyScore score_fuzzy(std::string_view query_folded, std::string_view text_folded,
                       std::string_view text_original) {
  FuzzyScore fs;
  if (query_folded.empty()) {
    fs.matched = true;
    return fs;
  }
  if (text_folded == query_folded) {
    fs.matched = true;
    fs.score = 1000;
    return fs;
  }
  if (text_folded.size() >= query_folded.size() &&
      text_folded.compare(0, query_folded.size(), query_folded) == 0) {
    fs.matched = true;
    fs.score = 860;
    return fs;
  }
  auto pos = text_folded.find(query_folded);
  if (pos != std::string_view::npos) {
    fs.matched = true;
    fs.score = 700 - static_cast<int>(pos) * 2;
    if (pos > 0) {
      char prev = text_folded[pos - 1];
      if (prev == ' ' || prev == '-' || prev == '_' || prev == '.') fs.score += 80;
    }
    return fs;
  }
  if (acronym_match(query_folded, text_original)) {
    fs.matched = true;
    fs.score = 640;
    return fs;
  }
  if (!is_subsequence(query_folded, text_folded)) {
    int d = levenshtein_bounded(query_folded, text_folded.substr(0, std::min(text_folded.size(),
                                                                             query_folded.size() + 2)),
                                2);
    if (d <= 2) {
      fs.matched = true;
      fs.score = 320 - d * 60;
    }
    return fs;
  }
  // Sublime-style consecutive bonus.
  int score = 0;
  int consec = 0;
  std::size_t qi = 0;
  for (std::size_t i = 0; i < text_folded.size() && qi < query_folded.size(); ++i) {
    if (text_folded[i] == query_folded[qi]) {
      ++consec;
      score += 12 + consec * 8;
      if (i == 0) score += 40;
      else {
        char p = text_folded[i - 1];
        if (p == ' ' || p == '-' || p == '_' || p == '/' || p == '\\' || p == '.') score += 28;
      }
      ++qi;
    } else {
      consec = 0;
      score -= 1;
    }
  }
  if (qi == query_folded.size()) {
    fs.matched = true;
    fs.score = std::max(40, score - static_cast<int>(text_folded.size() - query_folded.size()));
  }
  return fs;
}

}  // namespace wilfred
