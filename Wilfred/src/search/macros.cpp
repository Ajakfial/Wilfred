#include "wilfred/search/macros.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

namespace wilfred {

std::unordered_map<std::string, std::string> builtin_macros() {
  return {
      {"yt", "https://www.youtube.com/results?search_query={query}"},
      {"youtube", "https://www.youtube.com/results?search_query={query}"},
      {"g", "https://www.google.com/search?q={query}"},
      {"google", "https://www.google.com/search?q={query}"},
      {"bing", "https://www.bing.com/search?q={query}"},
      {"ddg", "https://duckduckgo.com/?q={query}"},
      {"gh", "https://github.com/search?q={query}"},
      {"github", "https://github.com/search?q={query}"},
      {"gist", "https://gist.github.com/search?q={query}"},
      {"wiki", "https://en.wikipedia.org/wiki/Special:Search?search={query}"},
      {"w", "https://en.wikipedia.org/wiki/Special:Search?search={query}"},
      {"maps", "https://www.google.com/maps/search/{query}"},
      {"gmaps", "https://www.google.com/maps/search/{query}"},
      {"so", "https://stackoverflow.com/search?q={query}"},
      {"imdb", "https://www.imdb.com/find?q={query}"},
      {"npm", "https://www.npmjs.com/search?q={query}"},
      {"pypi", "https://pypi.org/search/?q={query}"},
      {"crates", "https://crates.io/search?q={query}"},
      {"mdn", "https://developer.mozilla.org/search?q={query}"},
      {"define", "https://www.google.com/search?q=define+{query}"},
      {"translate", "https://translate.google.com/?sl=auto&tl=en&text={query}"},
      {"mail", "https://mail.google.com/mail/u/0/#search/{query}"},
      {"amazon", "https://www.amazon.com/s?k={query}"},
      {"reddit", "https://www.reddit.com/search/?q={query}"},
      {"tw", "https://x.com/search?q={query}"},
      {"x", "https://x.com/search?q={query}"},
      {"images", "https://www.google.com/search?tbm=isch&q={query}"},
      {"wolfram", "https://www.wolframalpha.com/input?i={query}"},
      {"arch", "https://wiki.archlinux.org/index.php?search={query}"},
      {"hn", "https://hn.algolia.com/?q={query}"},
      {"gclip", "https://www.google.com/search?q={clipboard_enc}"},
      {"clipsearch", "https://www.google.com/search?q={clipboard_enc}"},
      {"ytclip", "https://www.youtube.com/results?search_query={clipboard_enc}"},
  };
}

std::unordered_map<std::string, std::string> merged_macros(const Config& cfg) {
  auto m = builtin_macros();
  for (auto& [k, v] : cfg.macros) m[to_lower_utf8(k)] = v;
  return m;
}

static void trim_copy(std::string& s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
}

MacroMatch match_macro(std::string_view query, const Config& cfg) {
  MacroMatch out;
  if (!cfg.search.macros) return out;
  std::string s(query);
  trim_copy(s);
  if (s.empty()) return out;
  if (s[0] == '!' || s[0] == '/') s.erase(s.begin());
  trim_copy(s);
  auto macros = merged_macros(cfg);
  auto colon = s.find(':');
  auto space = s.find(' ');
  std::string name;
  std::string arg;
  if (colon != std::string::npos && (space == std::string::npos || colon < space) && colon > 0 &&
      colon < 24) {
    name = to_lower_utf8(s.substr(0, colon));
    arg = s.substr(colon + 1);
    trim_copy(arg);
  } else if (space != std::string::npos) {
    name = to_lower_utf8(s.substr(0, space));
    arg = s.substr(space + 1);
    trim_copy(arg);
  } else {
    name = to_lower_utf8(s);
  }
  auto it = macros.find(name);
  if (it == macros.end()) return out;
  // Bare names that are also common file queries stay as file search unless
  // they used bang/slash/colon syntax.
  if (arg.empty() && space == std::string::npos && colon == std::string::npos &&
      query.find('!') == std::string_view::npos && query.find('/') != 0) {
    static const char* weak[] = {"w", "so", "mail", "g", "x", nullptr};
    for (auto** p = weak; *p; ++p)
      if (name == *p) return out;
  }
  out.matched = true;
  out.name = name;
  out.tmpl = it->second;
  out.argument = arg;
  return out;
}

std::string expand_macro(const std::string& tmpl, const std::string& argument,
                         const std::string& clipboard) {
  std::string q = argument;
  auto replace = [&](const std::string& key, const std::string& val) {
    std::string o;
    std::size_t i = 0;
    while (i < tmpl.size()) {
      auto p = tmpl.find(key, i);
      if (p == std::string::npos) {
        o.append(tmpl, i, std::string::npos);
        break;
      }
      o.append(tmpl, i, p - i);
      o += val;
      i = p + key.size();
    }
    return o;
  };
  std::string enc = percent_encode(q);
  std::string clip_enc = percent_encode(clipboard);
  std::string out = tmpl;
  // Expand in a stable order; encoded forms first so {query} does not eat {query_enc}.
  auto subst = [](std::string s, const std::string& key, const std::string& val) {
    std::string o;
    std::size_t i = 0;
    while (i < s.size()) {
      auto p = s.find(key, i);
      if (p == std::string::npos) {
        o.append(s, i, std::string::npos);
        break;
      }
      o.append(s, i, p - i);
      o += val;
      i = p + key.size();
    }
    return o;
  };
  (void)replace;
  out = subst(out, "{clipboard_enc}", clip_enc);
  out = subst(out, "{clip_enc}", clip_enc);
  out = subst(out, "{query_enc}", enc);
  out = subst(out, "{q_enc}", enc);
  out = subst(out, "{clipboard}", clipboard);
  out = subst(out, "{clip}", clipboard);
  out = subst(out, "{query}", enc);
  out = subst(out, "{q}", enc);
  return out;
}

std::vector<SearchResult> macro_results(const MacroMatch& m, const std::string& clipboard) {
  std::vector<SearchResult> out;
  if (!m.matched) return out;
  SearchResult r;
  auto url = expand_macro(m.tmpl, m.argument, clipboard);
  if (m.argument.empty()) {
    r.title = "Macro · " + m.name;
    r.subtitle = "Type more to search, or open " + m.name;
  } else {
    r.title = m.name + " · " + m.argument;
    r.subtitle = url;
  }
  r.path = url;
  r.payload = url;
  r.action = ResultAction::WebSearch;
  r.score = 9800;
  r.kind_label = "macro";
  r.category = "macro";
  out.push_back(std::move(r));
  return out;
}

}  // namespace wilfred
