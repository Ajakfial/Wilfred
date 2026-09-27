#include "wilfred/index/tokenizer.hpp"

#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace wilfred {

std::string fold_search(std::string_view s) { return to_lower_utf8(s); }

bool looks_binary(std::string_view bytes) {
  if (bytes.empty()) return false;
  auto n = std::min<std::size_t>(bytes.size(), 800);
  int ctrl = 0;
  int nul = 0;
  for (std::size_t i = 0; i < n; ++i) {
    unsigned char c = static_cast<unsigned char>(bytes[i]);
    if (c == 0) ++nul;
    if (c < 9 || (c > 13 && c < 32)) ++ctrl;
  }
  if (nul > 0) return true;
  return ctrl * 12 > static_cast<int>(n);
}

bool content_stopword(std::string_view tok) {
  static const char* w[] = {"the", "and", "for", "are", "but", "not", "you", "all", "can", "had",
                            "her", "was", "one", "our", "out", "has", "his", "how", "its", "may",
                            "from", "this", "that", "with", "have", "been", "were", "they", "will",
                            "your", "what", "when", "which", nullptr};
  for (auto** p = w; *p; ++p)
    if (tok == *p) return true;
  return tok.size() < 2;
}

std::vector<std::string> tokenize_content(std::string_view text, std::size_t max_tokens) {
  std::vector<std::string> out;
  if (looks_binary(text)) return out;
  auto toks = tokenize_name(text.substr(0, std::min<std::size_t>(text.size(), 256 * 1024)));
  out.reserve(std::min(max_tokens, toks.size()));
  for (auto& t : toks) {
    if (content_stopword(t)) continue;
    bool dup = false;
    for (auto& e : out)
      if (e == t) {
        dup = true;
        break;
      }
    if (dup) continue;
    out.push_back(std::move(t));
    if (out.size() >= max_tokens) break;
  }
  return out;
}

std::string content_snippet(std::string_view text, std::string_view query, std::size_t max_len) {
  if (text.empty()) return {};
  auto q = fold_search(query);
  std::string folded = fold_search(text.substr(0, std::min<std::size_t>(text.size(), 64 * 1024)));
  std::size_t pos = q.empty() ? 0 : folded.find(q);
  if (pos == std::string::npos) pos = 0;
  std::size_t start = pos > 40 ? pos - 40 : 0;
  while (start < text.size() && start > 0 && text[start] != '\n' && pos - start < 80) --start;
  if (start < text.size() && text[start] == '\n') ++start;
  std::string s(text.substr(start, std::min(max_len, text.size() - start)));
  for (char& c : s)
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
  return s;
}

std::string percent_encode(std::string_view s) {
  std::string o;
  o.reserve(s.size() * 3);
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      o.push_back(static_cast<char>(c));
    else if (c == ' ')
      o += "%20";
    else {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      o += buf;
    }
  }
  return o;
}

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
