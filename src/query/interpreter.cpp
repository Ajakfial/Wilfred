#include "wilfred/query/interpreter.hpp"

#include "wilfred/ai/assistant.hpp"
#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/math/expr.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/disktools.hpp"
#include "wilfred/search/macros.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/workflows.hpp"

namespace wilfred {

QueryInterpreter::QueryInterpreter(IndexEngine& index, SearchEngine& search, SnippetStore* snippets,
                                   PluginHost* plugins)
    : index_(index), search_(search), snippets_(snippets), plugins_(plugins) {}

InterpretedQuery QueryInterpreter::interpret(const std::string& query, const Config& cfg,
                                             HistoryStore* history) {
  return interpret_impl(query, cfg, history, true, nullptr, nullptr);
}

QueryInterpreter::FastResult QueryInterpreter::interpret_fast(const std::string& query,
                                                              const Config& cfg,
                                                              HistoryStore* history) {
  FastResult out;
  out.effective = query;
  out.iq = interpret_impl(query, cfg, history, false, &out.effective, &out.providers_apply);
  return out;
}

std::vector<SearchResult> QueryInterpreter::query_providers(const std::string& query,
                                                            const Config& cfg, std::size_t limit) {
  return providers_.query_all(query, cfg, limit);
}

void merge_provider_results(std::vector<SearchResult>& base, std::vector<SearchResult> extra) {
  // OS-index federation fills gaps: drop `os` hits whose path is already
  // listed (Wilfred's own index ranks first) and collapse repeats.
  if (extra.empty()) return;
  auto norm_path = [](const std::string& p) {
    std::string o = to_lower_utf8(p);
    for (char& c : o)
      if (c == '\\') c = '/';
    while (o.size() > 1 && o.back() == '/')
      o.pop_back();
    return o;
  };
  std::vector<std::string> seen;
  seen.reserve(base.size());
  for (auto& r : base) {
    auto key = r.path.empty() ? r.payload : r.path;
    if (!key.empty()) seen.push_back(norm_path(key));
  }
  for (auto& r : extra) {
    auto key = r.path.empty() ? r.payload : r.path;
    if (r.category != "os") {
      // Exact repeats (same card twice, e.g. a cached attach racing its own
      // background push) collapse; distinct hits always pass through.
      bool dup = false;
      if (!key.empty()) {
        for (auto& s : base) {
          auto sk = s.path.empty() ? s.payload : s.path;
          if (s.category == r.category && s.title == r.title && sk == key) {
            dup = true;
            break;
          }
        }
      }
      if (!dup) base.push_back(std::move(r));
      continue;
    }
    if (key.empty()) continue;
    auto n = norm_path(key);
    bool dup = false;
    for (auto& s : seen)
      if (s == n) {
        dup = true;
        break;
      }
    if (dup) continue;
    seen.push_back(n);
    base.push_back(std::move(r));
  }
}

InterpretedQuery QueryInterpreter::interpret_impl(const std::string& query, const Config& cfg,
                                                  HistoryStore* history, bool include_providers,
                                                  std::string* effective_out,
                                                  bool* providers_apply_out) {
  InterpretedQuery iq;
  iq.classification = classify_query(query);
  auto finish = [&]() -> InterpretedQuery {
    attach_result_actions(iq.results, cfg);
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
  if (effective_out) *effective_out = effective;

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

  // Screen-aware AI (`ai see ...`): capture first, ask with vision input.
  // Checked before plain `ai ...` since `ai see x` also matches that prefix.
  {
    std::string vprompt;
    if (ai_vision_is_request(effective, vprompt)) {
      AiAssistant ai;
      auto cards = ai.results_for_image(vprompt, cfg);
      iq.results.insert(iq.results.end(), cards.begin(), cards.end());
      if (!vprompt.empty() && ai.configured(cfg)) return finish();
      // Unconfigured or bare prefix: fall through to normal search as well.
    }
  }

  // Local AI assistant (`ai ...` / `ask ...`). Optional; needs ai.api_key.
  {
    std::string prompt;
    if (ai_query_is_request(effective, prompt)) {
      AiAssistant ai;
      auto cards = ai.results_for(prompt, cfg);
      iq.results.insert(iq.results.end(), cards.begin(), cards.end());
      if (!prompt.empty() && ai.configured(cfg)) return finish();
      // Unconfigured or bare prefix: fall through to normal search as well.
    }
  }

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
    if (m.color) {
      auto push_color = [&](const std::string& title, const char* kind) {
        if (title.empty()) return;
        SearchResult r;
        r.title = title;
        r.subtitle = std::string("Color ") + kind + " · " + iq.classification.text;
        r.payload = title;
        r.path = title;
        r.action = ResultAction::Convert;
        r.score = 10000 - static_cast<int>(iq.results.size());
        r.kind_label = "color";
        iq.results.push_back(std::move(r));
      };
      push_color(m.color_hex, "hex");
      push_color(m.color_rgb, "rgb");
      push_color(m.color_hsl, "hsl");
      if (iq.results.empty()) {
        SearchResult r;
        r.title = m.display;
        r.subtitle = "Color · " + iq.classification.text;
        r.payload = m.display;
        r.action = ResultAction::Convert;
        r.score = 10000;
        iq.results.push_back(std::move(r));
      }
      return finish();
    }
    SearchResult r;
    r.title = m.display;
    r.subtitle = (m.currency     ? "Currency · "
                  : m.datetime   ? "Date/time · "
                  : m.devutil    ? "Dev · "
                  : m.conversion ? "Metric conversion · "
                                 : "Calculator · ") +
                 iq.classification.text;
    r.payload = m.display;
    r.action = m.conversion ? ResultAction::Convert : ResultAction::Calculate;
    r.score = 10000;
    if (m.devutil) {
      auto l = to_lower_utf8(iq.classification.text);
      if (l.rfind("uuid", 0) == 0 || l.rfind("guid", 0) == 0)
        r.kind_label = "uuid";
      else if (l.rfind("base64", 0) == 0 || l.rfind("b64", 0) == 0 || l.rfind("decode64", 0) == 0 ||
               l.rfind("encode64", 0) == 0)
        r.kind_label = "base64";
      else if (l.rfind("sha256", 0) == 0 || l.rfind("sha ", 0) == 0 || l.rfind("hash ", 0) == 0)
        r.kind_label = "sha256";
      else if (l.rfind("lorem", 0) == 0 || l.rfind("ipsum", 0) == 0)
        r.kind_label = "lorem";
      else if (l.rfind("json", 0) == 0 || l.rfind("pretty", 0) == 0)
        r.kind_label = "json";
      else
        r.kind_label = "dev";
    }
    iq.results.push_back(r);
    return finish();
  }

  if (iq.classification.kind == QueryKind::Command) {
    // `run <workflow>` takes precedence over shell-command search when the
    // remainder names a configured workflow.
    {
      std::string wf_name;
      if (is_workflow_query(effective, cfg, wf_name) && !wf_name.empty()) {
        iq.classification.kind = QueryKind::Mini;
        iq.results = workflow_results(wf_name, cfg);
        return finish();
      }
    }
    auto l = to_lower_utf8(effective);
    std::string rest = effective;
    if (l.rfind(">", 0) == 0)
      rest = effective.substr(1);
    else if (l.rfind("cmd ", 0) == 0)
      rest = effective.substr(4);
    else if (l.rfind("run ", 0) == 0)
      rest = effective.substr(4);
    while (!rest.empty() && rest.front() == ' ')
      rest.erase(rest.begin());
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

  auto minis = mini_results(effective, cfg, clip.text, &index_);
  if (!minis.empty()) {
    iq.classification.kind = QueryKind::Mini;
    iq.results.insert(iq.results.end(), minis.begin(), minis.end());
    auto intent = parse_mini_intent(effective);
    if (intent.exact) return finish();
  }

  // Parameterized quicklinks (`ql name args`, `name:args`, `!name args`).
  // Runs alongside macros; explicit invocations stop the pipeline.
  {
    auto ql = match_quicklink(effective, cfg);
    if (ql.matched) {
      iq.classification.kind = QueryKind::Macro;
      auto cards = quicklink_results(ql, clip.text);
      iq.results.insert(iq.results.end(), cards.begin(), cards.end());
      bool explicit_form = !effective.empty() && (effective[0] == '!' || effective[0] == '/');
      auto ll = to_lower_utf8(effective);
      if (ll.rfind("ql ", 0) == 0 || ll.rfind("quicklink ", 0) == 0 || ll.rfind("link ", 0) == 0)
        explicit_form = true;
      if (!ql.args.empty() || explicit_form) {
        if (iq.results.size() >= static_cast<std::size_t>(cfg.search.max_results)) return finish();
        // Bare `name args` quicklinks still allow file hits below (non-exact).
        if (explicit_form && !ql.args.empty()) {
          // Keep file hits too unless the list is already full; quicklink stays on top.
        }
      }
    }
  }

  // Workflows named on the command line without the `workflow` prefix are rare,
  // but `run <name>` is handled by the Workflow mini above. No extra step needed
  // here beyond minis; keep file search for non-exact minis.

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

  if (providers_apply_out) *providers_apply_out = true;
  if (include_providers) {
    auto extra = query_providers(effective, cfg, limit);
    merge_provider_results(iq.results, std::move(extra));
  }

  if (plugins_ && cfg.search.plugins && cfg.plugins.enabled) {
    auto plug = plugins_->query(effective, cfg, limit);
    iq.results.insert(iq.results.end(), plug.begin(), plug.end());
  }

  (void)index_;
  return finish();
}

bool result_is_launchable(const SearchResult& r) {
  return r.action != ResultAction::Habit && r.action != ResultAction::Calculate &&
         r.action != ResultAction::Convert && r.action != ResultAction::None &&
         r.action != ResultAction::SwitchWindow && r.action != ResultAction::System &&
         r.action != ResultAction::Screenshot && r.category != "window" &&
         r.category != "screenshot";
}

bool execute_result(const SearchResult& r, const Config& cfg, const std::string& action_id) {
  return execute_result_action(r, cfg, action_id);
}

}  // namespace wilfred
