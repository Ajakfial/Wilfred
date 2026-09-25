#include "wilfred/index/tokenizer.hpp"

#include "wilfred/core/utf8.hpp"

#include <cctype>

namespace wilfred {

std::string fold_search(std::string_view s) { return to_lower_utf8(s); }

std::vector<std::string> tokenize_name(std::string_view name) {
  std::vector<std::string> tokens;
  std::string cur;
  auto flush = [&] {
    if (!cur.empty()) {
      tokens.push_back(to_lower_utf8(cur));
      cur.clear();
    }
  };
  for (std::size_t i = 0; i < name.size(); ++i) {
    unsigned char c = static_cast<unsigned char>(name[i]);
    bool alnum = std::isalnum(c) != 0;
    if (alnum) {
      if (!cur.empty() && std::islower(static_cast<unsigned char>(cur.back())) &&
          std::isupper(c)) {
        flush();
      }
      cur.push_back(static_cast<char>(c));
    } else {
      flush();
    }
  }
  flush();
  return tokens;
}

std::string acronym_of(std::string_view name) {
  auto toks = tokenize_name(name);
  std::string a;
  for (auto& t : toks)
    if (!t.empty()) a.push_back(t[0]);
  return a;
}

std::vector<std::string> trigrams(std::string_view folded) {
  std::vector<std::string> t;
  std::string s = "  ";
  s.append(folded);
  s.push_back(' ');
  if (s.size() < 3) return t;
  t.reserve(s.size() - 2);
  for (std::size_t i = 0; i + 2 < s.size(); ++i) t.push_back(s.substr(i, 3));
  return t;
}

bool glob_match(std::string_view text, std::string_view pattern) {
  auto t = to_lower_utf8(text);
  auto p = to_lower_utf8(pattern);
  std::vector<std::vector<char>> dp(t.size() + 1, std::vector<char>(p.size() + 1, 0));
  dp[0][0] = 1;
  for (std::size_t j = 1; j <= p.size(); ++j)
    if (p[j - 1] == '*') dp[0][j] = dp[0][j - 1];
  for (std::size_t i = 1; i <= t.size(); ++i) {
    for (std::size_t j = 1; j <= p.size(); ++j) {
      if (p[j - 1] == '*')
        dp[i][j] = dp[i][j - 1] || dp[i - 1][j];
      else if (p[j - 1] == '?' || p[j - 1] == t[i - 1])
        dp[i][j] = dp[i - 1][j - 1];
    }
  }
  return dp[t.size()][p.size()] != 0;
}

}  // namespace wilfred
