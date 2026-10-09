#include "wilfred/search/setup.hpp"

#include "wilfred/config/settings.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/updater/updater.hpp"

#include <cctype>
#include <cstdio>
#include <iostream>

namespace wilfred {
namespace {

SearchResult scard(const std::string& title, const std::string& sub, const std::string& payload,
                   const std::string& label, const std::string& category) {
  SearchResult r;
  r.title = title;
  r.subtitle = sub;
  r.payload = payload;
  r.path = payload;
  r.action = ResultAction::Mini;
  r.score = 10000;
  r.kind_label = label;
  r.category = category;
  return r;
}

std::string trim_ws(const std::string& s) {
  std::string o = s;
  while (!o.empty() && (o.front() == ' ' || o.front() == '\t')) o.erase(o.begin());
  while (!o.empty() && (o.back() == ' ' || o.back() == '\t' || o.back() == '\r')) o.pop_back();
  return o;
}

// Compact double formatting for settings display ("1.000000" -> "1").
std::string fmt_num(double v) {
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.6f", v);
  std::string o = buf;
  while (o.size() > 1 && o.back() == '0' && o.find('.') != std::string::npos) o.pop_back();
  if (!o.empty() && o.back() == '.') o.pop_back();
  return o.empty() ? "0" : o;
}

std::string join_csv_out(const std::vector<std::string>& items) {
  std::string o;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i) o += ", ";
    o += items[i];
  }
  return o;
}

bool ranking_get(const RankingWeights& w, const std::string& name, std::string& out) {
  static const struct {
    const char* n;
    int RankingWeights::*m;
  } k[] = {
      {"exact_name", &RankingWeights::exact_name},
      {"prefix_name", &RankingWeights::prefix_name},
      {"substring_name", &RankingWeights::substring_name},
      {"fuzzy_name", &RankingWeights::fuzzy_name},
      {"acronym", &RankingWeights::acronym},
      {"path_component", &RankingWeights::path_component},
      {"extension", &RankingWeights::extension},
      {"application", &RankingWeights::application},
      {"recency", &RankingWeights::recency},
      {"frequency", &RankingWeights::frequency},
      {"previous_selection", &RankingWeights::previous_selection},
      {"word_boundary", &RankingWeights::word_boundary},
      {"token_proximity", &RankingWeights::token_proximity},
      {"directory_bonus", &RankingWeights::directory_bonus},
      {"alias", &RankingWeights::alias},
      {"learned_choice", &RankingWeights::learned_choice},
      {"context_parent", &RankingWeights::context_parent},
      {"context_extension", &RankingWeights::context_extension},
      {"access_recency", &RankingWeights::access_recency},
      {"clipboard_overlap", &RankingWeights::clipboard_overlap},
      {"content_hit", &RankingWeights::content_hit},
      {"hour_affinity", &RankingWeights::hour_affinity},
      {"pinned", &RankingWeights::pinned},
      {nullptr, nullptr},
  };
  for (auto* p = k; p->n; ++p)
    if (name == p->n) {
      out = std::to_string(w.*(p->m));
      return true;
    }
  return false;
}

// Flat getter table for config_get_value (a ~150-branch if/else chain hits
// MSVC's C1061 nesting limit — keep this table-driven).
using CfgGet = std::string (*)(const Config&);
static std::string g_bool(bool b) { return b ? "true" : "false"; }
#define GB(m) \
  [](const Config& c) -> std::string { return g_bool(c.m); }
#define GI(m) \
  [](const Config& c) -> std::string { return std::to_string(c.m); }
#define GD(m) \
  [](const Config& c) -> std::string { return fmt_num(c.m); }
#define GS(m) \
  [](const Config& c) -> std::string { return c.m; }
#define GL(m) \
  [](const Config& c) -> std::string { return join_csv_out(c.m); }
#define G0 \
  [](const Config&) -> std::string { return std::string(); }

static const struct {
  const char* key;
  CfgGet get;
} kGet[] = {
    {"search.max_results", GI(search.max_results)},
    {"search.debounce_ms", GI(search.debounce_ms)},
    {"search.min_query_length", GI(search.min_query_length)},
    {"search.include_system_files", GB(search.include_system_files)},
    {"search.include_hidden_files", GB(search.include_hidden_files)},
    {"search.show_system_in_results", GB(search.show_system_in_results)},
    {"search.web_search_fallback", GB(search.web_search_fallback)},
    {"search.treat_urls_as_open", GB(search.treat_urls_as_open)},
    {"search.fuzzy", GB(search.fuzzy)},
    {"search.acronyms", GB(search.acronyms)},
    {"search.context_aware", GB(search.context_aware)},
    {"search.clipboard", GB(search.clipboard)},
    {"search.minis", GB(search.minis)},
    {"search.macros", GB(search.macros)},
    {"search.snippets", GB(search.snippets)},
    {"search.plugins", GB(search.plugins)},
    {"browser.provider", GS(browser.provider)},
    {"browser.search_template", GS(browser.search_template)},
    {"browser.library", GB(browser.library)},
    {"hotkey.key", GS(hotkey.key)},
    {"hotkey.enabled", GB(hotkey.enabled)},
    {"hotkey.use_command_on_macos", GB(hotkey.use_command_on_macos)},
    {"hotkey.modifiers", GL(hotkey.modifiers)},
    {"ui.theme", GS(ui.theme)},
    {"ui.max_visible", GI(ui.max_visible)},
    {"ui.width", GI(ui.width)},
    {"ui.accent", GS(ui.accent)},
    {"ui.font_size", GI(ui.font_size)},
    {"ui.language", GS(ui.language)},
    {"logging.level", GS(logging.level)},
    {"logging.max_file_bytes", GI(logging.max_file_bytes)},
    {"logging.file", GS(logging.file)},
    {"plugins.enabled", GB(plugins.enabled)},
    {"plugins.directories", GL(plugins.directories)},
    {"plugins.require_approval", GB(plugins.require_approval)},
    {"plugins.timeout_ms", GI(plugins.timeout_ms)},
    {"plugins.registry", GS(plugins.registry)},
    {"providers.semantic", GB(providers.semantic)},
    {"providers.semantic_min_score", GD(providers.semantic_min_score)},
    {"providers.semantic_vector_weight", GD(providers.semantic_vector_weight)},
    {"providers.semantic_trigram_weight", GD(providers.semantic_trigram_weight)},
    {"providers.semantic_backend", GS(providers.semantic_backend)},
    {"providers.semantic_max_results", GI(providers.semantic_max_results)},
    {"embedding.enabled", GB(embedding.enabled)},
    {"embedding.min_score", GD(embedding.min_score)},
    {"embedding.backend", GS(embedding.backend)},
    {"embedding.model", GS(embedding.model)},
    {"embedding.endpoint", GS(embedding.endpoint)},
    {"embedding.dim", GI(embedding.dim)},
    {"embedding.max_results", GI(embedding.max_results)},
    {"ai.enabled", GB(ai.enabled)},
    {"ai.temperature", GD(ai.temperature)},
    {"ai.api_key", G0},
    {"ai.provider", GS(ai.provider)},
    {"ai.model", GS(ai.model)},
    {"ai.endpoint", GS(ai.endpoint)},
    {"ai.max_tokens", GI(ai.max_tokens)},
    {"ai.timeout_ms", GI(ai.timeout_ms)},
    {"sources.calendar", GB(sources.calendar)},
    {"sources.contacts", GB(sources.contacts)},
    {"sources.notes", GB(sources.notes)},
    {"sources.ocr", GB(sources.ocr)},
    {"sources.calendar_paths", GL(sources.calendar_paths)},
    {"sources.contacts_paths", GL(sources.contacts_paths)},
    {"sources.notes_paths", GL(sources.notes_paths)},
    {"sources.max_results", GI(sources.max_results)},
    {"sources.ocr_languages", GS(sources.ocr_languages)},
    {"transcription.enabled", GB(transcription.enabled)},
    {"transcription.binary", GS(transcription.binary)},
    {"transcription.model", GS(transcription.model)},
    {"transcription.save_txt", GB(transcription.save_txt)},
    {"transcription.mic", GS(transcription.mic)},
    {"transcription.language", GS(transcription.language)},
    {"api.enabled", GB(api.enabled)},
    {"api.port", GI(api.port)},
    {"api.token", G0},
    {"api.bind", GS(api.bind)},
    {"sync.enabled", GB(sync.enabled)},
    {"sync.encrypt", GB(sync.encrypt)},
    {"sync.key_file", GS(sync.key_file)},
    {"sync.password", G0},
    {"sync.token", G0},
    {"sync.interval_seconds", GI(sync.interval_seconds)},
    {"sync.include_index", GB(sync.include_index)},
    {"sync.url", GS(sync.url)},
    {"snippets.expansion", GB(snippets.expansion)},
    {"snippets.auto_paste", GB(snippets.auto_paste)},
    {"snippets.global_expansion", GB(snippets.global_expansion)},
    {"snippets.prefix", GS(snippets.prefix)},
    {"layouts.auto_apply", GB(layouts.auto_apply)},
    {"layouts.auto_layout", GS(layouts.auto_layout)},
    {"remotes.enabled", GB(remotes.enabled)},
    {"remotes.timeout_ms", GI(remotes.timeout_ms)},
    {"remotes.max_results", GI(remotes.max_results)},
    {"packages.enabled", GB(packages.enabled)},
    {"packages.max_results", GI(packages.max_results)},
    {"packages.timeout_ms", GI(packages.timeout_ms)},
    {"os_search.enabled", GB(os_search.enabled)},
    {"os_search.max_results", GI(os_search.max_results)},
    {"os_search.timeout_ms", GI(os_search.timeout_ms)},
    {"os_search.backend", GS(os_search.backend)},
    {"history.enabled", GB(history.enabled)},
    {"history.max_entries", GI(history.max_entries)},
    {"history.persist", GB(history.persist)},
    {"clipboard.manager", GB(clipboard.manager)},
    {"clipboard.max_entries", GI(clipboard.max_entries)},
    {"clipboard.persist", GB(clipboard.persist)},
    {"index.follow_symlinks", GB(index.follow_symlinks)},
    {"index.format", GS(index.format)},
    {"index.index_hidden", GB(index.index_hidden)},
    {"index.index_system", GB(index.index_system)},
    {"index.usn_scan", GB(index.usn_scan)},
    {"index.content_indexing", GB(index.content_indexing)},
    {"index.batch_size", GI(index.batch_size)},
    {"index.debounce_fs_ms", GI(index.debounce_fs_ms)},
    {"index.rescan_interval_seconds", GI(index.rescan_interval_seconds)},
    {"index.persist_every_records", GI(index.persist_every_records)},
    {"index.content_max_tokens", GI(index.content_max_tokens)},
    {"index.exclude", GL(index.exclude)},
    {"index.exclude_globs", GL(index.exclude_globs)},
    {"index.system_directories", GL(index.system_directories)},
    {"index.paths", GL(index.paths)},
    {"index.workers", GI(index.workers)},
    {"index.cpu_percent_limit", GI(index.cpu_percent_limit)},
    {"index.memory_limit_mb", GI(index.memory_limit_mb)},
    {"pins", GL(pins)},
    {nullptr, nullptr},
};

#undef GB
#undef GI
#undef GD
#undef GS
#undef GL
#undef G0

}  // namespace

std::vector<SearchResult> setup_results(const std::string& remainder, const Config& cfg) {
  std::vector<SearchResult> out;
  auto l = to_lower_utf8(trim_ws(remainder));
  bool roots_ok = !cfg.index.paths.empty() || !default_index_roots().empty();
  bool hotkey_ok = !cfg.hotkey.key.empty();
  bool browser_ok = cfg.browser.search_template.find("{query}") != std::string::npos;
  if (l.empty() || l == "status" || l == "check") {
    out.push_back(scard("Setup: " + setup_summary(cfg), "setup status — enter opens wilfred.yml",
                        "config:open", "setup", "setup"));
    out.push_back(scard(roots_ok ? "Index roots: configured" : "Index roots: using defaults",
                        "setup roots — edit index.paths in wilfred.yml", "config:open", "setup",
                        "setup"));
    out.push_back(scard("Hotkey: " + cfg.hotkey.key, "setup hotkey — edit hotkey: in wilfred.yml",
                        "config:open", "setup", "setup"));
    out.push_back(
        scard(browser_ok ? "Browser search: configured" : "Browser search: broken",
              "setup browser — edit browser.search_template", "config:open", "setup", "setup"));
    out.push_back(scard("Validate wilfred.yml", "setup validate — enter checks now",
                        "config:validate", "setup", "setup"));
    out.push_back(scard("Run terminal setup", "run: wilfred setup — prompts for roots/hotkey/browser",
                        "setup:run", "setup", "setup"));
    (void)hotkey_ok;
    return out;
  }
  if (l == "roots" || l == "root" || l == "index") {
    out.push_back(scard("Index roots: " + std::to_string(cfg.index.paths.size()) + " custom",
                        "edit index.paths in wilfred.yml — enter opens", "config:open", "setup",
                        "setup"));
    return out;
  }
  if (l == "hotkey" || l == "hotkeys" || l == "key") {
    out.push_back(scard("Hotkey: " + cfg.hotkey.key, "edit hotkey: — enter opens wilfred.yml",
                        "config:open", "setup", "setup"));
    return out;
  }
  if (l == "browser" || l == "search") {
    out.push_back(scard("Browser template: " + cfg.browser.search_template,
                        "edit browser.search_template — enter opens", "config:open", "setup",
                        "setup"));
    return out;
  }
  if (l == "validate" || l == "check config" || l == "config") {
    std::string msg;
    std::string text;
    bool ok = false;
    if (read_file_all(cfg.source_path.empty() ? default_config_path() : cfg.source_path, text))
      ok = validate_config_text(text, msg);
    else
      msg = "cannot read " + (cfg.source_path.empty() ? default_config_path() : cfg.source_path);
    out.push_back(scard(ok ? "Config valid" : "Config invalid: " + msg,
                        cfg.source_path.empty() ? default_config_path() : cfg.source_path,
                        "config:validate", "setup", "setup"));
    return out;
  }
  // Default: show status.
  out.push_back(scard("Setup: " + setup_summary(cfg), "setup — enter opens wilfred.yml",
                      "config:open", "setup", "setup"));
  return out;
}

std::vector<SearchResult> update_results(const std::string& remainder, const Config& cfg) {
  (void)remainder;
  (void)cfg;
  std::vector<SearchResult> out;
  PendingUpdate p = pending_update();
  if (p.available) {
    out.push_back(scard("Update available: " + p.current_version + " → " + p.latest_version,
                        "enter copies download link · run `wilfred update` to install",
                        p.download_url.empty() ? p.latest_version : p.download_url, "update",
                        "update"));
    return out;
  }
  if (p.checked) {
    std::string cur = p.current_version.empty() ? wilfred_version() : p.current_version;
    out.push_back(scard("Wilfred up to date (" + cur + ")",
                        "enter copies version · `wilfred update --check` re-checks now", cur,
                        "update", "update"));
    return out;
  }
  out.push_back(scard("Checking for updates…",
                      "background check runs at startup · `wilfred update --check` checks now",
                      wilfred_version(), "update", "update"));
  return out;
}

std::vector<SearchResult> config_results(const std::string& remainder, const Config& cfg) {
  std::vector<SearchResult> out;
  auto l = to_lower_utf8(trim_ws(remainder));
  std::string path = cfg.source_path.empty() ? default_config_path() : cfg.source_path;
  if (l.empty() || l == "open" || l == "edit" || l == "show") {
    out.push_back(scard("Edit wilfred.yml", path + " — enter opens in editor", "config:open",
                        "config", "config"));
    out.push_back(scard("Show in folder", path + " — enter reveals", "config:reveal", "config",
                        "config"));
    out.push_back(scard("Validate wilfred.yml", "enter checks syntax + schema", "config:validate",
                        "config", "config"));
    out.push_back(scard("Copy config path", path, "config:copy", "config", "config"));
    out.push_back(scard("Set a setting", "config set search.max_results 40 — enter applies",
                        "config:open", "config", "config"));
    out.push_back(scard("Reset to defaults", "backs up to .pre-reset.bak", "config:reset",
                        "config", "config"));
    return out;
  }
  if (l == "validate" || l == "check" || l == "verify" || l == "lint") {
    std::string text, msg;
    bool ok = false;
    if (read_file_all(path, text))
      ok = validate_config_text(text, msg);
    else
      msg = "cannot read " + path;
    out.push_back(scard(ok ? "Config valid" : "Config error: " + msg, path, "config:validate",
                        "config", "config"));
    return out;
  }
  if (l == "reveal" || l == "folder" || l == "locate") {
    out.push_back(scard("Show wilfred.yml in folder", path, "config:reveal", "config", "config"));
    return out;
  }
  if (l == "path" || l == "where" || l == "copy") {
    out.push_back(scard(path, "config path — enter copies", "config:copy", "config", "config"));
    return out;
  }
  if (l == "reset" || l == "restore defaults" || l == "defaults") {
    out.push_back(scard("Reset wilfred.yml to defaults",
                        "backs up to .pre-reset.bak — enter resets", "config:reset", "config",
                        "config"));
    return out;
  }
  // `config set <section.key> <value>` / `config get <key>`.
  {
    std::string low = l;
    if (low.rfind("set ", 0) == 0 || low.rfind("edit ", 0) == 0) {
      auto rest = trim_ws(remainder.substr(remainder.find(' ') + 1));
      auto sp = rest.find(' ');
      if (sp == std::string::npos) {
        out.push_back(scard("Usage: config set <section.key> <value>",
                            "e.g. config set search.max_results 40", "", "config", "config"));
        return out;
      }
      auto key = trim_ws(rest.substr(0, sp));
      auto val = trim_ws(rest.substr(sp + 1));
      std::string cur, err;
      ConfigError cerr;
      Config live = cfg;
      bool known = config_get_value(live, to_lower_utf8(key), cur, err);
      out.push_back(scard("Set " + key + " to " + val,
                          known ? ("now: " + cur + " — enter applies") : err + " — enter tries anyway",
                          "config:set:" + key + "=" + val, "config", "config"));
      return out;
    }
    if (low.rfind("get ", 0) == 0) {
      auto key = to_lower_utf8(trim_ws(remainder.substr(4)));
      std::string cur, err;
      if (config_get_value(cfg, key, cur, err))
        out.push_back(scard(key + " = " + cur, path + " — enter copies", "config:get:" + key,
                            "config", "config"));
      else
        out.push_back(scard("Unknown setting: " + key, err, "", "config", "config"));
      return out;
    }
  }
  out.push_back(scard("Edit wilfred.yml", path + " — enter opens", "config:open", "config",
                      "config"));
  return out;
}

bool validate_config_text(const std::string& text, std::string& message) {
  Config c;
  ConfigError err;
  if (!load_config_text(text, c, err)) {
    message = err.message.empty() ? "invalid config" : err.message;
    return false;
  }
  message = "ok";
  return true;
}

std::string setup_summary(const Config& cfg) {
  int missing = 0;
  if (cfg.index.paths.empty() && default_index_roots().empty()) ++missing;
  if (cfg.hotkey.key.empty()) ++missing;
  if (cfg.browser.search_template.find("{query}") == std::string::npos) ++missing;
  if (missing == 0) return "ready (roots, hotkey, browser ok)";
  return std::to_string(missing) + " item(s) need attention — open wilfred.yml";
}

static std::string prompt_line(const std::string& prompt, const std::string& def) {
  std::cout << prompt;
  if (!def.empty()) std::cout << " [" << def << "]";
  std::cout << ": ";
  std::string line;
  std::getline(std::cin, line);
  if (line.empty()) return def;
  return line;
}

int run_setup_wizard(bool overwrite) {
  ConfigError err;
  Config cfg = load_or_create_user_config(err);
  if (!err.message.empty()) std::cout << "note: " << err.message << "\n";
  std::string path = cfg.source_path.empty() ? default_config_path() : cfg.source_path;
  std::cout << "Wilfred setup — " << path << "\n";
  std::cout << "Leaves existing keys untouched unless --overwrite is given.\n\n";

  std::string text;
  if (!read_file_all(path, text)) {
    std::cerr << "cannot read " << path << "\n";
    return 1;
  }
  bool changed = false;
  auto ensure_key = [&](const std::string& section, const std::string& key,
                        const std::string& value_line) {
    // Very small YAML-aware append: if `key:` is absent under `section:`,
    // insert it. Never touches existing keys unless overwrite.
    if (text.find(key + ":") != std::string::npos && !overwrite) return;
    auto sec = text.find(section + ":");
    if (sec == std::string::npos) {
      text += "\n" + section + ":\n  " + value_line + "\n";
      changed = true;
      return;
    }
    auto nl = text.find('\n', sec);
    if (nl == std::string::npos) nl = text.size();
    text.insert(nl + 1, "  " + value_line + "\n");
    changed = true;
  };

  std::cout << "1/3 Index roots (comma-separated folders, empty = OS defaults).\n";
  std::string roots = prompt_line("  roots", "");
  if (!roots.empty()) {
    // Minimal quoting: wrap each root in double quotes.
    std::string cur;
    std::string list;
    for (char c : roots + ",") {
      if (c == ',') {
        auto t = trim_ws(cur);
        if (!t.empty()) {
          if (!list.empty()) list += ", ";
          list += "\"" + t + "\"";
        }
        cur.clear();
      } else {
        cur.push_back(c);
      }
    }
    if (!list.empty()) {
      ensure_key("index", "paths", "paths: [" + list + "]");
    }
  }

  std::cout << "\n2/3 Global hotkey key (single letter, default W = Ctrl+Alt+W).\n";
  std::string key = prompt_line("  key", cfg.hotkey.key.empty() ? "W" : cfg.hotkey.key);
  if (!key.empty() && (overwrite || cfg.hotkey.key.empty() || key != cfg.hotkey.key)) {
    // Handled via full-file note: hotkey.key lives under hotkey:.
    // Only append when the file lacks an explicit key.
    if (text.find("hotkey:") == std::string::npos || overwrite) {
      ensure_key("hotkey", "key", "key: " + key);
    } else if (text.find("key:") == std::string::npos) {
      ensure_key("hotkey", "key", "key: " + key);
    } else {
      std::cout << "  (keeping existing hotkey.key — use --overwrite to change)\n";
    }
  }

  std::cout << "\n3/3 Browser search template (must contain {query}).\n";
  std::string tmpl = prompt_line("  template", cfg.browser.search_template);
  if (!tmpl.empty() && tmpl.find("{query}") == std::string::npos) {
    std::cout << "  template must contain {query} — skipped.\n";
  } else if (!tmpl.empty() && (overwrite || tmpl != cfg.browser.search_template)) {
    if (text.find("search_template:") == std::string::npos || overwrite) {
      ensure_key("browser", "search_template", "search_template: \"" + tmpl + "\"");
    } else {
      std::cout << "  (keeping existing browser.search_template — use --overwrite to change)\n";
    }
  }

  if (!changed) {
    std::cout << "\nNothing to change — " << setup_summary(cfg) << "\n";
    return 0;
  }
  std::string msg;
  if (!validate_config_text(text, msg)) {
    std::cerr << "refusing to write: " << msg << "\n";
    return 1;
  }
  if (!write_file_atomic(path, text.data(), text.size())) {
    std::cerr << "cannot write " << path << "\n";
    return 1;
  }
  std::cout << "\nWrote " << path << " — restart the daemon to apply.\n";
  return 0;
}

int run_config_validate() {
  std::string path = default_config_path();
  std::string text;
  if (!read_file_all(path, text)) {
    std::cerr << "cannot read " << path << "\n";
    return 1;
  }
  std::string msg;
  if (!validate_config_text(text, msg)) {
    std::cerr << "invalid: " << msg << "\n";
    return 1;
  }
  std::cout << "valid: " << path << "\n";
  return 0;
}

int run_config_open() {
  std::string path = default_config_path();
  if (!native_open_editor(path) && !native_launch(path)) {
    std::cerr << "cannot open " << path << "\n";
    return 1;
  }
  return 0;
}

int run_config_path() {
  std::cout << default_config_path() << "\n";
  return 0;
}

namespace {

// --- Typed settings editor ---------------------------------------------

enum class SetType { Bool, Int, Double, Str, List };

struct SetSpec {
  SetType type;
};

bool setting_spec(const std::string& dotted, SetSpec& spec) {
  SettingMeta meta{"", SettingType::Str, false, false, {}};
  if (!setting_meta(dotted, meta)) return false;
  switch (meta.type) {
    case SettingType::Bool:
      spec.type = SetType::Bool;
      break;
    case SettingType::Int:
      spec.type = SetType::Int;
      break;
    case SettingType::Double:
      spec.type = SetType::Double;
      break;
    case SettingType::List:
      spec.type = SetType::List;
      break;
    default:
      spec.type = SetType::Str;
      break;
  }
  return true;
}

std::string split_section(const std::string& dotted, std::string& key) {
  auto pos = dotted.find('.');
  if (pos == std::string::npos || pos == 0 || pos + 1 >= dotted.size()) {
    key.clear();
    return {};
  }
  key = dotted.substr(pos + 1);
  return dotted.substr(0, pos);
}

std::string yaml_quote(const std::string& s) {
  if (s.empty()) return "\"\"";
  bool need = false;
  for (char c : s) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '.' || c == '_' ||
          c == '-' || c == '+')) {
      need = true;
      break;
    }
  }
  if (!need) return s;
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\') o.push_back('\\');
    o.push_back(c);
  }
  o.push_back('"');
  return o;
}

std::vector<std::string> split_csv(const std::string& s) {
  std::vector<std::string> out;
  std::string cur;
  for (char c : s + ",") {
    if (c == ',') {
      auto t = trim_ws(cur);
      if (!t.empty()) out.push_back(t);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  return out;
}

// Replace (or insert) `key:` under `section:` in YAML text. Returns false
// with error when the section/key cannot be represented.
bool yaml_set_key(std::string& text, const std::string& section, const std::string& key,
                  const std::string& rendered, std::string& error) {
  // Find the section header at column 0.
  std::size_t sec_pos = std::string::npos;
  {
    std::size_t pos = 0;
    while (pos < text.size()) {
      auto nl = text.find('\n', pos);
      std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
      if (line == section + ":") {
        sec_pos = pos;
        break;
      }
      if (nl == std::string::npos) break;
      pos = nl + 1;
    }
  }
  if (sec_pos == std::string::npos) {
    if (!text.empty() && text.back() != '\n') text.push_back('\n');
    text += section + ":\n  " + key + ": " + rendered + "\n";
    return true;
  }
  // Scan lines under the section (indented) for the key.
  std::size_t pos = text.find('\n', sec_pos);
  pos = pos == std::string::npos ? text.size() : pos + 1;
  while (pos < text.size()) {
    auto nl = text.find('\n', pos);
    std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    if (!line.empty() && line[0] != ' ' && line[0] != '\t' && line[0] != '#' &&
        line.back() == ':') {
      // Next top-level section: insert before it.
      text.insert(pos, "  " + key + ": " + rendered + "\n");
      return true;
    }
    // Match `  key:` with any leading indent.
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
    if (line.compare(i, key.size(), key) == 0) {
      std::size_t j = i + key.size();
      while (j < line.size() && (line[j] == ' ' || line[j] == '\t')) ++j;
      if (j < line.size() && line[j] == ':') {
        std::string indent = line.substr(0, i);
        std::string replacement = indent + key + ": " + rendered;
        text.replace(pos, line.size(), replacement);
        return true;
      }
    }
    if (nl == std::string::npos) break;
    pos = nl + 1;
  }
  if (!text.empty() && text.back() != '\n') text.push_back('\n');
  text += "  " + key + ": " + rendered + "\n";
  (void)error;
  return true;
}

}  // namespace

bool config_get_value(const Config& cfg, const std::string& dotted, std::string& out,
                      std::string& error) {
  SetSpec spec;
  if (!setting_spec(dotted, spec)) {
    error = "unknown setting '" + dotted + "' (try: search.max_results, browser.search_template)";
    return false;
  }
  // Format from the loaded struct so output always reflects reality.
  if (dotted.rfind("ranking.", 0) == 0) {
    if (!ranking_get(cfg.ranking, dotted.substr(8), out)) {
      error = "setting '" + dotted + "' is readable via wilfred.yml only";
      return false;
    }
    return true;
  }
  for (auto* g = kGet; g->key; ++g)
    if (dotted == g->key) {
      out = g->get(cfg);
      return true;
    }
  error = "setting '" + dotted + "' is readable via wilfred.yml only";
  return false;
}

bool config_set_value(const std::string& dotted, const std::string& value, std::string& error) {
  error.clear();
  SetSpec spec;
  if (!setting_spec(dotted, spec)) {
    error = "unknown setting '" + dotted + "' (try: search.max_results, browser.search_template)";
    return false;
  }
  std::string key;
  std::string section = split_section(dotted, key);
  if (section.empty() || key.empty()) {
    error = "use section.key, e.g. search.max_results";
    return false;
  }
  std::string rendered;
  if (spec.type == SetType::Bool) {
    auto l = to_lower_utf8(trim_ws(value));
    if (l == "true" || l == "1" || l == "yes" || l == "on") rendered = "true";
    else if (l == "false" || l == "0" || l == "no" || l == "off") rendered = "false";
    else {
      error = "'" + dotted + "' wants true/false";
      return false;
    }
  } else if (spec.type == SetType::Int) {
    try {
      (void)std::stoll(trim_ws(value));
      rendered = trim_ws(value);
    } catch (...) {
      error = "'" + dotted + "' wants an integer";
      return false;
    }
  } else if (spec.type == SetType::Double) {
    try {
      (void)std::stod(trim_ws(value));
      rendered = trim_ws(value);
    } catch (...) {
      error = "'" + dotted + "' wants a number";
      return false;
    }
  } else if (spec.type == SetType::List) {
    auto items = split_csv(value);
    rendered = "[";
    for (std::size_t i = 0; i < items.size(); ++i) {
      if (i) rendered += ", ";
      rendered += yaml_quote(items[i]);
    }
    rendered += "]";
  } else {
    rendered = yaml_quote(trim_ws(value));
  }
  std::string path = default_config_path();
  std::string text;
  if (!read_file_all(path, text)) {
    error = "cannot read " + path;
    return false;
  }
  if (!yaml_set_key(text, section, key, rendered, error)) return false;
  std::string msg;
  if (!validate_config_text(text, msg)) {
    error = msg;
    return false;
  }
  if (!write_file_atomic(path, text.data(), text.size())) {
    error = "cannot write " + path;
    return false;
  }
  return true;
}

bool config_reset_default(std::string& error) {
  error.clear();
  auto path = default_config_path();
  std::string cur;
  if (read_file_all(path, cur)) {
    auto bak = path + ".pre-reset.bak";
    write_file_atomic(bak, cur.data(), cur.size());
  }
  const char* candidates[] = {"config/wilfred.default.yml",
                              "/usr/share/wilfred/wilfred.default.yml",
                              "/usr/local/share/wilfred/wilfred.default.yml"};
  for (auto* c : candidates) {
    std::string shipped;
    if (read_file_all(c, shipped) && !shipped.empty()) {
      if (!write_file_atomic(path, shipped.data(), shipped.size())) {
        error = "cannot write " + path;
        return false;
      }
      return true;
    }
  }
  error = "shipped default not found (reinstall or delete " + path + " to regenerate)";
  return false;
}

int run_config_get(const std::string& dotted) {
  ConfigError err;
  Config cfg = load_or_create_user_config(err);
  std::string out, msg;
  if (!config_get_value(cfg, dotted, out, msg)) {
    std::cerr << msg << "\n";
    return 1;
  }
  std::cout << out << "\n";
  return 0;
}

int run_config_set(const std::string& dotted, const std::string& value) {
  std::string err;
  if (!config_set_value(dotted, value, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  std::cout << dotted << " updated — restart the daemon to apply.\n";
  return 0;
}

int run_config_reset() {
  std::string err;
  if (!config_reset_default(err)) {
    std::cerr << err << "\n";
    return 1;
  }
  std::cout << "reset to defaults (previous saved as .pre-reset.bak) — restart the daemon.\n";
  return 0;
}

}  // namespace wilfred
