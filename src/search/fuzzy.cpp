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
  for (std::size_t j = 0; j <= b.size(); ++j)
    prev[j] = static_cast<int>(j);
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

int typo_threshold(std::size_t len) {
  if (len < 3) return 0;
  if (len < 5) return 1;
  if (len < 9) return 2;
  return 3;
}

int damerau_bounded(std::string_view a, std::string_view b, int max_dist) {
  if (a == b) return 0;
  auto n = a.size();
  auto m = b.size();
  if (n == 0) return m <= static_cast<std::size_t>(max_dist) ? static_cast<int>(m) : max_dist + 1;
  if (m == 0) return n <= static_cast<std::size_t>(max_dist) ? static_cast<int>(n) : max_dist + 1;
  if (static_cast<int>(n > m ? n - m : m - n) > max_dist) return max_dist + 1;
  // Optimal string alignment (restricted Damerau-Levenshtein) with adjacent
  // transposition. Bounded + early exit; inputs are short (queries, tokens).
  std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1, 0));
  for (std::size_t i = 0; i <= n; ++i)
    d[i][0] = static_cast<int>(i);
  for (std::size_t j = 0; j <= m; ++j)
    d[0][j] = static_cast<int>(j);
  for (std::size_t i = 1; i <= n; ++i) {
    int row_min = max_dist + 1;
    for (std::size_t j = 1; j <= m; ++j) {
      int cost = a[i - 1] == b[j - 1] ? 0 : 1;
      int v = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
        v = std::min(v, d[i - 2][j - 2] + 1);
      d[i][j] = v;
      row_min = std::min(row_min, v);
    }
    // Early exit: best possible in later rows can't beat max_dist if the
    // whole row already exceeds it by more than the remaining rows allow.
    // Conservative check keeps correctness for short strings.
    if (row_min > max_dist) {
      // Verify no future row can recover (insertions only add cost).
      bool recoverable = false;
      for (std::size_t j = 0; j <= m; ++j) {
        if (d[i][j] + static_cast<int>(n - i) <= max_dist) {
          recoverable = true;
          break;
        }
      }
      if (!recoverable) return max_dist + 1;
    }
  }
  int dist = d[n][m];
  return dist <= max_dist ? dist : max_dist + 1;
}

bool is_typo_match(std::string_view query, std::string_view candidate) {
  if (query.empty() || candidate.empty()) return false;
  if (query == candidate) return true;
  std::size_t len = std::min(query.size(), candidate.size());
  int thr = typo_threshold(len);
  if (thr <= 0) return false;
  if (static_cast<int>(query.size() > candidate.size() ? query.size() - candidate.size()
                                                       : candidate.size() - query.size()) > thr)
    return false;
  // Require same first character for distance-2 matches to avoid wild
  // corrections ("time" vs "dime" style false positives are still distance 1
  // and allowed; distance 2 without shared prefix is rejected).
  int d = damerau_bounded(query, candidate, thr);
  if (d > thr) return false;
  if (d == 1) return true;
  return !query.empty() && !candidate.empty() && query[0] == candidate[0];
}

bool is_typo_prefix(std::string_view query, std::string_view candidate) {
  if (query.empty() || candidate.empty() || candidate.size() < query.size()) return false;
  auto head = candidate.substr(0, query.size());
  if (head == query) return true;
  return is_typo_match(query, head);
}

int typo_score(std::string_view query_folded, std::string_view text_folded) {
  if (query_folded.empty() || text_folded.empty()) return 0;
  int thr = typo_threshold(std::min(query_folded.size(), text_folded.size()));
  if (thr <= 0) return 0;
  int d = damerau_bounded(query_folded, text_folded, thr);
  if (d > thr) return 0;
  // 1000 for exact (handled elsewhere), ~800/600/400 for d=1/2/3.
  int base = 900 - d * 220;
  // Prefer same-length (likely true typo) over length mismatch.
  int len_diff = static_cast<int>(query_folded.size() > text_folded.size()
                                      ? query_folded.size() - text_folded.size()
                                      : text_folded.size() - query_folded.size());
  base -= len_diff * 40;
  // Shared prefix is a strong typo signal ("firefoz" vs "firefox").
  std::size_t common = 0;
  while (common < query_folded.size() && common < text_folded.size() &&
         query_folded[common] == text_folded[common])
    ++common;
  base += static_cast<int>(common) * 12;
  return std::max(0, base);
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
    // Typo-tolerant path: Damerau (handles transpositions like "fireofx"
    // -> "firefox") on a head window, plus full-token comparison for short
    // names. Scores sit below substring (700) but well above noise so typo
    // hits survive ranking floors.
    int thr = typo_threshold(query_folded.size());
    if (thr > 0) {
      std::size_t win = std::min(text_folded.size(), query_folded.size() + 2);
      int d = damerau_bounded(query_folded, text_folded.substr(0, win), thr);
      if (d <= thr) {
        fs.matched = true;
        fs.score = 420 - d * 90;
        if (!query_folded.empty() && !text_folded.empty() && query_folded[0] == text_folded[0])
          fs.score += 40;
        return fs;
      }
      // Token-level typo: "visaul" should match token "visual" inside
      // "Visual Studio Code" even when the full-string distance is large.
      auto toks = tokenize_name(text_original);
      int best_d = thr + 1;
      for (auto& t : toks) {
        if (t.size() < 3) continue;
        if (static_cast<int>(t.size() > query_folded.size() ? t.size() - query_folded.size()
                                                            : query_folded.size() - t.size()) > thr)
          continue;
        int td = damerau_bounded(query_folded, std::string_view(t), thr);
        if (td < best_d) best_d = td;
      }
      if (best_d <= thr) {
        fs.matched = true;
        fs.score = 380 - best_d * 80;
        return fs;
      }
    }
    int d = levenshtein_bounded(
        query_folded, text_folded.substr(0, std::min(text_folded.size(), query_folded.size() + 2)),
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
      if (i == 0)
        score += 40;
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
