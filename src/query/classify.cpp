#include "wilfred/query/classify.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/classify.hpp"
#include "wilfred/math/expr.hpp"

#include <cctype>

namespace wilfred {

bool looks_like_url(std::string_view s) {
  auto t = std::string(s);
  while (!t.empty() && t.back() == ' ') t.pop_back();
  while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
  auto l = to_lower_utf8(t);
  if (l.rfind("http://", 0) == 0 || l.rfind("https://", 0) == 0 || l.rfind("file://", 0) == 0)
    return true;
  if (l.rfind("www.", 0) == 0) return true;
  if (t.find('\\') != std::string::npos) return false;
  auto slash = t.find('/');
  auto host = slash == std::string::npos ? t : t.substr(0, slash);
  auto dot = host.find_last_of('.');
  if (dot == std::string::npos || dot == 0 || dot + 1 >= host.size()) return false;
  auto ext = path_extension(host);
  if (!ext.empty()) {
    auto k = classify_extension(ext);
    if (k != FileKind::File && k != FileKind::Unknown) return false;
  }
  auto tld = to_lower_utf8(host.substr(dot + 1));
  static const char* tlds[] = {"com", "org",  "net", "edu", "gov", "io",  "co",  "uk", "de",
                               "app", "dev",  "info", "biz", "us", "ca", "au", "jp", "fr",
                               "ru",  "cn",   "nl",  "se",  "no",  "fi",  "es", "it", "br",
                               "xyz", "me",   "tv",  nullptr};
  bool ok = false;
  for (auto** p = tlds; *p; ++p)
    if (tld == *p) {
      ok = true;
      break;
    }
  if (!ok) return false;
  for (char c : host) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-')) return false;
  }
  return host.find('.') != std::string::npos;
}

bool looks_like_math(std::string_view s) {
  std::string t(s);
  if (t.find(" to ") != std::string::npos || t.find(" in ") != std::string::npos ||
      to_lower_utf8(t).rfind("convert ", 0) == 0) {
    auto m = evaluate_math(t);
    return m.ok;
  }
  bool has_op = false;
  bool has_digit = false;
  bool has_letter_word = false;
  int letters = 0;
  for (char c : t) {
    if (std::isdigit(static_cast<unsigned char>(c))) has_digit = true;
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%' || c == '(' ||
        c == ')')
      has_op = true;
    if (std::isalpha(static_cast<unsigned char>(c))) ++letters;
  }
  if (!has_digit) return false;
  if (!has_op && letters == 0) return false;  // bare numbers are not calculator queries
  static const char* fn[] = {"sin",  "cos",  "tan", "sqrt", "log", "ln",  "abs",
                             "pow",  "ceil", "floor", "exp", "pi", nullptr};
  auto l = to_lower_utf8(t);
  bool fnish = false;
  for (auto** p = fn; *p; ++p)
    if (l.find(*p) != std::string::npos) fnish = true;
  if (!has_op && !fnish) return false;
  auto m = evaluate_math(t);
  (void)has_letter_word;
  return m.ok;
}

QueryClass classify_query(std::string_view raw) {
  QueryClass q;
  std::string s(raw);
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
  q.text = s;
  if (s.empty()) {
    q.kind = QueryKind::Empty;
    return q;
  }
  auto l = to_lower_utf8(s);
  if (l.rfind("?", 0) == 0 || l.rfind("g ", 0) == 0 || l.rfind("web ", 0) == 0 ||
      l.rfind("search ", 0) == 0) {
    q.kind = QueryKind::WebSearch;
    q.confident = true;
    if (l.rfind("?", 0) == 0)
      q.remainder = s.substr(1);
    else if (l.rfind("g ", 0) == 0)
      q.remainder = s.substr(2);
    else if (l.rfind("web ", 0) == 0)
      q.remainder = s.substr(4);
    else
      q.remainder = s.substr(7);
    return q;
  }
  if (looks_like_url(s)) {
    q.kind = QueryKind::Url;
    q.confident = true;
    return q;
  }
  if (looks_like_math(s)) {
    q.kind = QueryKind::Math;
    q.confident = true;
    return q;
  }
  if (l.rfind(">", 0) == 0 || l.rfind("cmd ", 0) == 0 || l.rfind("run ", 0) == 0) {
    q.kind = QueryKind::Command;
    q.confident = true;
    return q;
  }
  if (s.find(':') != std::string::npos || s.find("*.") != std::string::npos) {
    q.kind = QueryKind::FilteredSearch;
    return q;
  }
  q.kind = QueryKind::FileSearch;
  return q;
}

}  // namespace wilfred
