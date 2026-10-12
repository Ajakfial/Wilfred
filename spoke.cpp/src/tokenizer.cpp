#include "spoke/tokenizer.h"
#include <cctype>

namespace spoke {

std::string EnglishTokenizer::normalize(const std::string& text) const {
  std::string out;
  out.reserve(text.size());
  bool last_space = true;  // trim leading
  for (char ch : text) {
    unsigned char u = static_cast<unsigned char>(ch);
    char c = static_cast<char>(std::tolower(u));
    if (c >= 'a' && c <= 'z') {
      out.push_back(c);
      last_space = false;
    } else if (c == '\'' || c == '\'') {
      out.push_back('\'');
      last_space = false;
    } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '-' || c == '_') {
      if (!last_space) {
        out.push_back(' ');
        last_space = true;
      }
    }
    // drop everything else (punctuation, digits handled as drop)
  }
  while (!out.empty() && out.back() == ' ') out.pop_back();
  return out;
}

std::vector<int> EnglishTokenizer::encode(const std::string& text) const {
  std::string n = normalize(text);
  std::vector<int> ids;
  ids.reserve(n.size());
  for (char c : n) ids.push_back(charToId(c));
  return ids;
}

std::string EnglishTokenizer::decode(const std::vector<int>& ids) const {
  std::string out;
  out.reserve(ids.size());
  for (int id : ids) {
    if (id == kBlank) continue;
    out.push_back(idToChar(id));
  }
  return out;
}

char EnglishTokenizer::idToChar(int id) const {
  if (id >= 1 && id <= 26) return static_cast<char>('a' + id - 1);
  if (id == 27) return ' ';
  if (id == 28) return '\'';
  return '?';
}

int EnglishTokenizer::charToId(char c) const {
  if (c >= 'a' && c <= 'z') return (c - 'a') + 1;
  if (c >= 'A' && c <= 'Z') return (c - 'A') + 1;
  if (c == ' ') return 27;
  if (c == '\'') return 28;
  return 27;  // map unknowns to space (never blank)
}

}  // namespace spoke
