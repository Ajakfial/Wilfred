#include "wilfred/query/interpreter.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/math/expr.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/macros.hpp"
#include "wilfred/search/minis.hpp"

namespace wilfred {

QueryInterpreter::QueryInterpreter(IndexEngine& index, SearchEngine& search, SnippetStore* snippets,
                                   PluginHost* plugins)
    : index_(index), search_(search), snippets_(snippets), plugins_(plugins) {}

InterpretedQuery QueryInterpreter::interpret(const std::string& query, const Config& cfg,
                                             HistoryStore* history) {
  InterpretedQuery iq;
  iq.classification = classify_query(query);
  auto finish = [&]() -> InterpretedQuery {
    attach_result_actions(iq.results);
    return iq;
  };

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
      return finish();
    }
  }

  auto add_habits = [&](const std::string& prefix, int n) {
    if (!history || !history->enabled()) return;
    for (auto& h : history->suggest_queries(prefix, n)) {
      SearchResult s;
      s.title = h;
      s.subtitle = "Typed often";
      s.action = ResultAction::Habit;
      s.score = 1;
      iq.results.push_back(std::move(s));
    }
  };

  ClipboardSnapshot clip;
  if (cfg.search.clipboard) clip = read_clipboard();

  std::string snip_name;
  if (snippets_ && cfg.search.snippets && query_is_snippet_save(effective, snip_name)) {
    Snippet s;
    s.id = snip_name;
    s.trigger = snip_name;
    s.title = snip_name;
    s.body = clip.text;
    s.kind = "clip";
    snippets_->upsert(std::move(s));
    snippets_->save();
    SearchResult r;
    r.title = clip.text.empty() ? "Saved empty snippet \"" + snip_name + "\""
                                : "Saved snippet \"" + snip_name + "\"";
    r.subtitle = "Type ;" + snip_name + " or snip " + snip_name + " to expand";
    r.path = snip_name;
    r.payload = clip.text;
    r.action = ResultAction::Expand;
    r.score = 10000;
    r.kind_label = "snippet";
    r.category = "snippet";
    iq.results.push_back(std::move(r));
    return finish();
  }

  if (snippets_ && cfg.search.snippets && cfg.snippets.expansion) {
    auto sn = snippets_->match(effective, cfg);
    if (snippet_query_forced(effective, cfg)) {
      iq.results = std::move(sn);
      return finish();
    }
    iq.results.insert(iq.results.end(), sn.begin(), sn.end());
  }

  if (normalize_query(effective).empty()) {
    add_habits("", 6);
    if (!clip.text.empty()) {
      SearchResult cr;
      cr.title = clipboard_preview(clip.text);
      cr.subtitle = "Clipboard · enter copies";
      cr.payload = clip.text;
      cr.path = clip.text;
      cr.action = ResultAction::Copy;
      cr.score = 8700;
      cr.kind_label = "clipboard";
      cr.category = "clipboard";
      iq.results.push_back(std::move(cr));
    }
    if (history && history->enabled()) {
      for (auto& [path, n] : history->top_selections(8)) {
        SearchResult r;
        r.title = path_stem(path);
        if (r.title.empty()) r.title = path;
        r.subtitle = path_parent(path);
        if (r.subtitle.empty()) r.subtitle = path;
        r.path = path;
        r.payload = path;
        r.action = ResultAction::Open;
        r.score = 800 + n;
        auto ext = to_lower_utf8(path_extension(path));
        if (ext == ".exe" || ext == ".lnk" || ext == ".app" || ext == ".desktop")
          r.kind = FileKind::Application;
        iq.results.push_back(std::move(r));
      }
    }
    return finish();
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
    return finish();
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
    auto found = search_.search(rest, cfg, history, limit);
    iq.results.insert(iq.results.end(), found.begin(), found.end());
    if (!iq.results.empty()) iq.results.front().action = ResultAction::Open;
    return finish();
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
    return finish();
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
    return finish();
  }

  auto minis = mini_results(effective, cfg, clip.text);
  if (!minis.empty()) {
    iq.classification.kind = QueryKind::Mini;
    iq.results.insert(iq.results.end(), minis.begin(), minis.end());
    auto intent = parse_mini_intent(effective);
    if (intent.exact && intent.remainder.empty()) return finish();
  }

  auto macro = match_macro(effective, cfg);
  if (macro.matched) {
    iq.classification.kind = QueryKind::Macro;
    auto cards = macro_results(macro, clip.text);
    iq.results.insert(iq.results.end(), cards.begin(), cards.end());
    if (!macro.argument.empty() || effective.find('!') != std::string::npos ||
        (!effective.empty() && effective[0] == '/')) {
      if (iq.results.size() >= static_cast<std::size_t>(cfg.search.max_results)) return finish();
    }
  }

  auto limit = static_cast<std::size_t>(cfg.search.max_results);
  add_habits(effective, 4);
  auto found = search_.search(effective, cfg, history, limit);
  iq.results.insert(iq.results.end(), found.begin(), found.end());

  if (cfg.search.web_search_fallback && !effective.empty()) {
    SearchResult web;
    web.title = "Search the web for \"" + effective + "\"";
    web.subtitle = "Web";
    web.payload = web_search_url(cfg.browser.search_template, effective);
    web.path = web.payload;
    web.action = ResultAction::WebSearch;
    web.score = 1;
    web.kind_label = "web";
    iq.results.push_back(web);
  }

  auto extra = providers_.query_all(effective, cfg, limit);
  iq.results.insert(iq.results.end(), extra.begin(), extra.end());

  if (plugins_ && cfg.search.plugins && cfg.plugins.enabled) {
    auto plug = plugins_->query(effective, cfg, limit);
    iq.results.insert(iq.results.end(), plug.begin(), plug.end());
  }

  (void)index_;
  return finish();
}

bool result_is_launchable(const SearchResult& r) {
  return r.action != ResultAction::Habit && r.action != ResultAction::Calculate &&
         r.action != ResultAction::Convert && r.action != ResultAction::None;
}

bool execute_result(const SearchResult& r, const Config& cfg, const std::string& action_id) {
  return execute_result_action(r, cfg, action_id);
}

}  // namespace wilfred
