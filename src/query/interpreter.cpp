#include "wilfred/query/interpreter.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/math/expr.hpp"

namespace wilfred {

QueryInterpreter::QueryInterpreter(IndexEngine& index, SearchEngine& search)
    : index_(index), search_(search) {}

InterpretedQuery QueryInterpreter::interpret(const std::string& query, const Config& cfg,
                                             HistoryStore* history) {
  InterpretedQuery iq;
  iq.classification = classify_query(query);

  auto alias_it = cfg.aliases.find(to_lower_utf8(query));
  std::string effective = query;
  if (alias_it != cfg.aliases.end()) {
    iq.classification.kind = QueryKind::Alias;
    effective = alias_it->second;
    if (effective == "default-browser") {
      SearchResult r;
      r.title = "Default browser";
      r.subtitle = default_browser_executable();
      r.path = r.subtitle;
      r.kind = FileKind::Application;
      r.action = ResultAction::Open;
      r.score = 10000;
      iq.results.push_back(r);
      return iq;
    }
  }

  if (iq.classification.kind == QueryKind::Math) {
    auto m = evaluate_math(iq.classification.text);
    SearchResult r;
    r.title = m.display;
    r.subtitle = (m.conversion ? "Metric conversion · " : "Calculator · ") + iq.classification.text;
    r.payload = m.display;
    r.action = m.conversion ? ResultAction::Convert : ResultAction::Calculate;
    r.score = 10000;
    iq.results.push_back(r);
    return iq;
  }

  if (iq.classification.kind == QueryKind::Command) {
    auto l = to_lower_utf8(effective);
    std::string rest = effective;
    if (l.rfind(">", 0) == 0) rest = effective.substr(1);
    else if (l.rfind("cmd ", 0) == 0)
      rest = effective.substr(4);
    else if (l.rfind("run ", 0) == 0)
      rest = effective.substr(4);
    while (!rest.empty() && rest.front() == ' ') rest.erase(rest.begin());
    auto limit = static_cast<std::size_t>(cfg.search.max_results);
    iq.results = search_.search(rest, cfg, history, limit);
    if (!iq.results.empty()) iq.results.front().action = ResultAction::Open;
    return iq;
  }

  if (iq.classification.kind == QueryKind::Url) {
    auto url = iq.classification.text;
    if (to_lower_utf8(url).rfind("www.", 0) == 0) url = "https://" + url;
    SearchResult r;
    r.title = "Open " + url;
    r.subtitle = "URL";
    r.path = url;
    r.payload = url;
    r.action = ResultAction::Open;
    r.score = 10000;
    iq.results.push_back(r);
    return iq;
  }

  if (iq.classification.kind == QueryKind::WebSearch) {
    auto q = iq.classification.remainder.empty() ? effective : iq.classification.remainder;
    SearchResult r;
    r.title = "Search the web for \"" + q + "\"";
    r.subtitle = default_browser_id();
    r.payload = web_search_url(cfg.browser.search_template, q);
    r.path = r.payload;
    r.action = ResultAction::WebSearch;
    r.score = 9000;
    iq.results.push_back(r);
    return iq;
  }

  auto limit = static_cast<std::size_t>(cfg.search.max_results);
  iq.results = search_.search(effective, cfg, history, limit);

  if (cfg.search.web_search_fallback && !effective.empty()) {
    SearchResult web;
    web.title = "Search the web for \"" + effective + "\"";
    web.subtitle = "Web";
    web.payload = web_search_url(cfg.browser.search_template, effective);
    web.path = web.payload;
    web.action = ResultAction::WebSearch;
    web.score = 1;
    iq.results.push_back(web);
  }

  auto extra = providers_.query_all(effective, cfg, limit);
  iq.results.insert(iq.results.end(), extra.begin(), extra.end());
  (void)index_;
  return iq;
}

bool execute_result(const SearchResult& r, const Config& cfg) {
  (void)cfg;
  if (r.action == ResultAction::Calculate || r.action == ResultAction::Convert) return true;
  if (r.action == ResultAction::WebSearch) return open_in_default_browser(r.payload);
  if (r.path.rfind("http://", 0) == 0 || r.path.rfind("https://", 0) == 0)
    return open_url(r.path);
  return launch_path(r.path.empty() ? r.payload : r.path);
}

}  // namespace wilfred
