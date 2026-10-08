#pragma once

#include <string>
#include <vector>

namespace wilfred {

struct DefineHit {
  std::string word;
  std::string pos;  // noun | verb | adjective | ...
  std::string definition;
  std::string synonyms;  // comma-joined
};

// Offline dictionary/thesaurus (no network). Returns false when unknown.
bool define_lookup(const std::string& word, DefineHit& out);
// Prefix suggestions for `define foo` completions.
std::vector<std::string> define_suggest(const std::string& prefix, std::size_t limit = 8);

}  // namespace wilfred
