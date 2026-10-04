#include "wilfred/search/setup.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/native.hpp"

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
  static const char* bools[] = {
      "search.include_system_files", "search.include_hidden_files",
      "search.show_system_in_results", "search.web_search_fallback",
      "search.treat_urls_as_open", "search.fuzzy", "search.acronyms",
      "search.context_aware", "search.clipboard", "search.minis",
      "search.macros", "search.snippets", "search.plugins",
      "index.follow_symlinks", "index.index_hidden", "index.index_system",
      "index.usn_scan", "index.content_indexing", "hotkey.enabled",
      "hotkey.use_command_on_macos", "browser.library", "plugins.enabled",
      "plugins.require_approval", "providers.semantic", "embedding.enabled",
      "ai.enabled", "sources.calendar", "sources.contacts", "sources.notes",
      "sources.ocr", "transcription.enabled", "transcription.save_txt",
      "api.enabled", "sync.enabled", "sync.include_index",
      "snippets.expansion", "snippets.auto_paste", "snippets.global_expansion",
      "layouts.auto_apply", "remotes.enabled", "history.enabled",
      "history.persist", "clipboard.manager", "clipboard.persist", nullptr};
  static const char* ints[] = {
      "search.max_results", "search.debounce_ms", "search.min_query_length",
      "index.workers", "index.cpu_percent_limit", "index.memory_limit_mb",
      "index.batch_size", "index.debounce_fs_ms", "index.rescan_interval_seconds",
      "index.persist_every_records", "ui.max_visible", "ui.width",
      "plugins.timeout_ms", "providers.semantic_max_results", "embedding.dim",
      "embedding.max_results", "ai.max_tokens", "ai.timeout_ms", "sources.max_results",
      "remotes.timeout_ms", "remotes.max_results", "api.port",
      "sync.interval_seconds", "history.max_entries", "clipboard.max_entries",
      "logging.max_file_bytes", "index.content_max_tokens", nullptr};
  static const char* doubles[] = {"providers.semantic_min_score", "embedding.min_score",
                                  "ai.temperature", nullptr};
  static const char* strs[] = {
      "browser.provider", "browser.search_template", "logging.level", "logging.file",
      "ui.theme", "hotkey.key", "embedding.backend", "embedding.model", "embedding.endpoint",
      "ai.provider", "ai.model", "ai.api_key", "ai.endpoint", "sources.ocr_languages",
      "transcription.binary", "transcription.model", "transcription.language",
      "transcription.mic", "api.bind", "api.token", "sync.url", "sync.token",
      "snippets.prefix", "layouts.auto_layout", "plugins.registry", nullptr};
  static const char* lists[] = {
      "index.paths", "index.exclude", "index.exclude_globs", "index.system_directories",
      "hotkey.modifiers", "plugins.directories", "sources.calendar_paths",
      "sources.contacts_paths", "sources.notes_paths", nullptr};
  for (auto** p = bools; *p; ++p)
    if (dotted == *p) {
      spec.type = SetType::Bool;
      return true;
    }
  for (auto** p = ints; *p; ++p)
    if (dotted == *p) {
      spec.type = SetType::Int;
      return true;
    }
  for (auto** p = doubles; *p; ++p)
    if (dotted == *p) {
      spec.type = SetType::Double;
      return true;
    }
  for (auto** p = strs; *p; ++p)
    if (dotted == *p) {
      spec.type = SetType::Str;
      return true;
    }
  for (auto** p = lists; *p; ++p)
    if (dotted == *p) {
      spec.type = SetType::List;
      return true;
    }
  return false;
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
  Config c = cfg;
  // Format from the loaded struct so output always reflects reality.
  if (dotted == "search.max_results") out = std::to_string(c.search.max_results);
  else if (dotted == "search.debounce_ms") out = std::to_string(c.search.debounce_ms);
  else if (dotted == "search.min_query_length") out = std::to_string(c.search.min_query_length);
  else if (dotted == "search.include_system_files") out = c.search.include_system_files ? "true" : "false";
  else if (dotted == "search.include_hidden_files") out = c.search.include_hidden_files ? "true" : "false";
  else if (dotted == "search.show_system_in_results") out = c.search.show_system_in_results ? "true" : "false";
  else if (dotted == "search.web_search_fallback") out = c.search.web_search_fallback ? "true" : "false";
  else if (dotted == "search.treat_urls_as_open") out = c.search.treat_urls_as_open ? "true" : "false";
  else if (dotted == "search.fuzzy") out = c.search.fuzzy ? "true" : "false";
  else if (dotted == "search.acronyms") out = c.search.acronyms ? "true" : "false";
  else if (dotted == "search.context_aware") out = c.search.context_aware ? "true" : "false";
  else if (dotted == "search.clipboard") out = c.search.clipboard ? "true" : "false";
  else if (dotted == "search.minis") out = c.search.minis ? "true" : "false";
  else if (dotted == "search.macros") out = c.search.macros ? "true" : "false";
  else if (dotted == "search.snippets") out = c.search.snippets ? "true" : "false";
  else if (dotted == "search.plugins") out = c.search.plugins ? "true" : "false";
  else if (dotted == "browser.provider") out = c.browser.provider;
  else if (dotted == "browser.search_template") out = c.browser.search_template;
  else if (dotted == "browser.library") out = c.browser.library ? "true" : "false";
  else if (dotted == "hotkey.key") out = c.hotkey.key;
  else if (dotted == "hotkey.enabled") out = c.hotkey.enabled ? "true" : "false";
  else if (dotted == "hotkey.use_command_on_macos") out = c.hotkey.use_command_on_macos ? "true" : "false";
  else if (dotted == "ui.theme") out = c.ui.theme;
  else if (dotted == "ui.max_visible") out = std::to_string(c.ui.max_visible);
  else if (dotted == "ui.width") out = std::to_string(c.ui.width);
  else if (dotted == "logging.level") out = c.logging.level;
  else if (dotted == "logging.file") out = c.logging.file;
  else if (dotted == "plugins.enabled") out = c.plugins.enabled ? "true" : "false";
  else if (dotted == "plugins.require_approval") out = c.plugins.require_approval ? "true" : "false";
  else if (dotted == "plugins.timeout_ms") out = std::to_string(c.plugins.timeout_ms);
  else if (dotted == "plugins.registry") out = c.plugins.registry;
  else if (dotted == "providers.semantic") out = c.providers.semantic ? "true" : "false";
  else if (dotted == "embedding.enabled") out = c.embedding.enabled ? "true" : "false";
  else if (dotted == "ai.enabled") out = c.ai.enabled ? "true" : "false";
  else if (dotted == "sources.calendar") out = c.sources.calendar ? "true" : "false";
  else if (dotted == "sources.contacts") out = c.sources.contacts ? "true" : "false";
  else if (dotted == "sources.notes") out = c.sources.notes ? "true" : "false";
  else if (dotted == "sources.ocr") out = c.sources.ocr ? "true" : "false";
  else if (dotted == "transcription.enabled") out = c.transcription.enabled ? "true" : "false";
  else if (dotted == "api.enabled") out = c.api.enabled ? "true" : "false";
  else if (dotted == "api.port") out = std::to_string(c.api.port);
  else if (dotted == "sync.enabled") out = c.sync.enabled ? "true" : "false";
  else if (dotted == "snippets.expansion") out = c.snippets.expansion ? "true" : "false";
  else if (dotted == "layouts.auto_apply") out = c.layouts.auto_apply ? "true" : "false";
  else if (dotted == "layouts.auto_layout") out = c.layouts.auto_layout;
  else if (dotted == "remotes.enabled") out = c.remotes.enabled ? "true" : "false";
  else if (dotted == "remotes.timeout_ms") out = std::to_string(c.remotes.timeout_ms);
  else if (dotted == "remotes.max_results") out = std::to_string(c.remotes.max_results);
  else if (dotted == "history.enabled") out = c.history.enabled ? "true" : "false";
  else if (dotted == "clipboard.manager") out = c.clipboard.manager ? "true" : "false";
  else if (dotted == "index.follow_symlinks") out = c.index.follow_symlinks ? "true" : "false";
  else if (dotted == "index.workers") out = std::to_string(c.index.workers);
  else if (dotted == "index.cpu_percent_limit") out = std::to_string(c.index.cpu_percent_limit);
  else if (dotted == "index.memory_limit_mb") out = std::to_string(c.index.memory_limit_mb);
  else {
    // Generic fallback for remaining scalars/lists.
    if (dotted == "index.paths") {
      for (std::size_t i = 0; i < c.index.paths.size(); ++i) {
        if (i) out += ", ";
        out += c.index.paths[i];
      }
    } else if (dotted == "hotkey.modifiers") {
      for (std::size_t i = 0; i < c.hotkey.modifiers.size(); ++i) {
        if (i) out += ", ";
        out += c.hotkey.modifiers[i];
      }
    } else if (dotted == "sources.ocr_languages") out = c.sources.ocr_languages;
    else if (dotted == "snippets.prefix") out = c.snippets.prefix;
    else if (dotted == "transcription.language") out = c.transcription.language;
    else if (dotted == "api.bind") out = c.api.bind;
    else if (dotted == "sync.url") out = c.sync.url;
    else {
      error = "setting '" + dotted + "' is readable via wilfred.yml only";
      return false;
    }
  }
  return true;
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
