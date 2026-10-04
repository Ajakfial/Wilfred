#include "wilfred/config/config.hpp"

#include "wilfred/config/yaml.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace wilfred {

static std::string norm_ext(std::string e) {
  if (e.empty()) return e;
  if (e[0] != '.') e.insert(e.begin(), '.');
  return to_lower_utf8(e);
}

static bool parse_level_ok(const std::string& s) {
  auto l = to_lower_utf8(s);
  return l == "error" || l == "warn" || l == "info" || l == "debug";
}

namespace {

// Levenshtein distance for "did you mean" suggestions. Case-insensitive;
// inputs are already lowercased by callers in most cases.
int edit_distance(const std::string& a, const std::string& b) {
  std::vector<int> prev(b.size() + 1), cur(b.size() + 1);
  for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = static_cast<int>(j);
  for (std::size_t i = 1; i <= a.size(); ++i) {
    cur[0] = static_cast<int>(i);
    for (std::size_t j = 1; j <= b.size(); ++j) {
      int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
      int del = prev[j] + 1;
      int ins = cur[j - 1] + 1;
      int sub = prev[j - 1] + cost;
      int best = del < ins ? del : ins;
      cur[j] = best < sub ? best : sub;
    }
    prev.swap(cur);
  }
  return prev[b.size()];
}

std::string suggest_key(const std::string& bad, const std::vector<std::string>& valid) {
  std::string best;
  int best_d = 100;
  auto lower_bad = to_lower_utf8(bad);
  for (auto& v : valid) {
    int d = edit_distance(lower_bad, to_lower_utf8(v));
    // Allow longer keys a bit more slack.
    int threshold = static_cast<int>(std::max<std::size_t>(2, v.size() / 3));
    if (d < best_d && d <= threshold) {
      best_d = d;
      best = v;
    }
  }
  return best;
}

std::string join_keys(const std::vector<std::string>& keys) {
  std::string o;
  for (std::size_t i = 0; i < keys.size(); ++i) {
    if (i) o += ", ";
    o += keys[i];
  }
  return o;
}

bool check_unknown_keys(const YamlValue& map, const std::vector<std::string>& valid,
                        const std::string& section, ConfigError& err) {
  if (!map.is_map()) return true;
  for (auto& [k, v] : map.as_map()) {
    (void)v;
    bool known = false;
    for (auto& ok : valid)
      if (ok == k) {
        known = true;
        break;
      }
    if (!known) {
      auto sug = suggest_key(k, valid);
      err.message = "Unknown key '" + k + "' in '" + section + ":'";
      if (!sug.empty()) err.message += ". Did you mean '" + sug + "'?";
      err.message += " Valid keys are: " + join_keys(valid) + ".";
      return false;
    }
  }
  return true;
}

bool is_bool_string(const std::string& s) {
  auto l = to_lower_utf8(s);
  // Trim whitespace.
  std::string t;
  for (char c : l)
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') t.push_back(c);
  return t == "true" || t == "false" || t == "yes" || t == "no" || t == "on" || t == "off" ||
         t == "1" || t == "0" || t == "y" || t == "n";
}

bool is_int_string(const std::string& s) {
  std::size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
  bool any = false;
  while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
    ++i;
    any = true;
  }
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  return any && i == s.size();
}

bool is_number_string(const std::string& s) {
  std::size_t i = 0;
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
  bool any = false;
  while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
    ++i;
    any = true;
  }
  if (i < s.size() && s[i] == '.') {
    ++i;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
      ++i;
      any = true;
    }
  }
  if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
    ++i;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) ++i;
    bool exp = false;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
      ++i;
      exp = true;
    }
    if (!exp) return false;
  }
  while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
  return any && i == s.size();
}

// Friendly type check: when a key is present, verify the raw YAML scalar
// looks like the expected type. The lenient getters (boolean/integer/number)
// silently fall back to defaults; this surfaces "expected integer, got 'abc'"
// with an example instead.
bool expect_bool(const YamlValue* section, const char* key, const std::string& sect,
                 ConfigError& err) {
  if (!section) return true;
  auto* v = section->get(key);
  if (!v || !v->is_string()) {
    if (v && !v->is_string()) {
      err.message = std::string("'") + sect + "." + key +
                    "' must be true/false. Example: " + key + ": true";
      return false;
    }
    return true;
  }
  if (!is_bool_string(v->as_string())) {
    err.message = std::string("'") + sect + "." + key + "' must be true/false, got '" +
                  v->as_string() + "'. Example: " + key + ": true";
    return false;
  }
  return true;
}

bool expect_int(const YamlValue* section, const char* key, const std::string& sect,
                ConfigError& err) {
  if (!section) return true;
  auto* v = section->get(key);
  if (!v || !v->is_string()) {
    if (v && !v->is_string()) {
      err.message = std::string("'") + sect + "." + key +
                    "' must be an integer. Example: " + key + ": 40";
      return false;
    }
    return true;
  }
  if (!is_int_string(v->as_string())) {
    err.message = std::string("'") + sect + "." + key + "' must be an integer, got '" +
                  v->as_string() + "'. Example: " + key + ": 40";
    return false;
  }
  return true;
}

[[maybe_unused]] bool expect_number(const YamlValue* section, const char* key, const std::string& sect,
                   ConfigError& err) {
  if (!section) return true;
  auto* v = section->get(key);
  if (!v || !v->is_string()) {
    if (v && !v->is_string()) {
      err.message = std::string("'") + sect + "." + key +
                    "' must be a number. Example: " + key + ": 0.5";
      return false;
    }
    return true;
  }
  if (!is_number_string(v->as_string())) {
    err.message = std::string("'") + sect + "." + key + "' must be a number, got '" +
                  v->as_string() + "'. Example: " + key + ": 0.5";
    return false;
  }
  return true;
}

std::string trim_flow(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.pop_back();
  return s;
}

// The minimal YAML parser stores flow collections (`[a, b]`) as plain strings.
// Expand them so `workflows:` / `app_actions:` accept both block lists:
//   review:
//     - copy_path
//     - reveal
// and flow / '+' forms:
//   review: [copy_path, reveal]
//   ship: "copy_path+reveal"
std::vector<std::string> split_flow_or_plus(const std::string& s) {
  std::string t = trim_flow(s);
  std::vector<std::string> out;
  if (t.size() >= 2 && t.front() == '[' && t.back() == ']') {
    std::string inner = t.substr(1, t.size() - 2);
    std::string cur;
    for (char ch : inner + ",") {
      if (ch == ',') {
        auto item = trim_flow(cur);
        // Strip surrounding quotes.
        if (item.size() >= 2 &&
            ((item.front() == '"' && item.back() == '"') ||
             (item.front() == '\'' && item.back() == '\'')))
          item = item.substr(1, item.size() - 2);
        item = trim_flow(item);
        if (!item.empty()) {
          // Flow items may themselves be '+' chains.
          std::string sub;
          for (char c2 : item + "+") {
            if (c2 == '+') {
              auto st = trim_flow(sub);
              if (!st.empty()) out.push_back(st);
              sub.clear();
            } else {
              sub.push_back(c2);
            }
          }
        }
        cur.clear();
      } else {
        cur.push_back(ch);
      }
    }
    return out;
  }
  std::string cur;
  for (char ch : t + "+") {
    if (ch == '+') {
      auto item = trim_flow(cur);
      if (!item.empty()) out.push_back(item);
      cur.clear();
    } else {
      cur.push_back(ch);
    }
  }
  return out;
}

bool is_valid_action_step(const std::string& s) {
  if (s.empty() || s.size() > 128) return false;
  bool has_alpha = false;
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == ':' || c == '-' || c == '+' || c == '.' || c == '/';
    if (!ok) return false;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_') has_alpha = true;
  }
  if (!has_alpha) return false;
  static const char* known[] = {"open",        "reveal",       "copy_path",  "copy_name",
                                "copy_posix",  "copy_file_uri", "copy_wsl",   "copy_text",
                                "copy",        "hash_file",    "compress_zip", "open_terminal",
                                "open_editor", "new_file",     "new_folder", "kill_process",
                                "timer_stop",  "paste",        "expand",     "clip_pin",
                                "clip_unpin",  "clip_clear",   "copy_name",  "transcribe_run",
                                "dictate_run", "convert_run", "bgremove_run",
                                "window_minimize", "window_maximize", "window_restore",
                                "window_close", "window_snap_left", "window_snap_right",
                                nullptr};
  std::string low;
  for (char c : s) low.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  // Allow prefixed families.
  if (low.rfind("open_with:", 0) == 0 || low.rfind("workflow:", 0) == 0 ||
      low.rfind("media:", 0) == 0 || low.rfind("note_delete:", 0) == 0 ||
      low.rfind("todo_done:", 0) == 0 || low.rfind("todo_undo:", 0) == 0 ||
      low.rfind("todo_delete:", 0) == 0 || low.rfind("layout_apply:", 0) == 0 ||
      low.rfind("tile:", 0) == 0 ||
      low.rfind("focus_window:", 0) == 0 || low.rfind("dictate_run:", 0) == 0)
    return true;
  for (auto** p = known; *p; ++p)
    if (low == *p) return true;
  return false;
}

}  // namespace

bool load_config_text(const std::string& text, Config& out, ConfigError& err) {
  err = {};
  YamlValue root;
  YamlError ye;
  if (!parse_yaml(text, root, ye)) {
    err.message = "YAML parse error at line " + std::to_string(ye.line) + ", column " +
                  std::to_string(ye.column) + ": " + ye.message;
    return false;
  }
  if (!root.is_map()) {
    err.message = "Configuration root must be a mapping";
    return false;
  }

  Config c;

  {
    std::vector<std::string> top_valid = {"search",   "index",   "ranking",  "aliases",
                                          "macros",   "scopes",  "custom_metadata", "history",
                                          "clipboard", "hotkey", "browser", "logging",
                                          "ui",       "plugins", "providers", "embedding",
                                          "ai",       "sources", "remotes", "packages", "layouts",
                                          "transcription", "api",
                                          "sync",     "snippets", "workflows", "quicklinks",
                                          "app_actions", "hotkeys"};
    if (!check_unknown_keys(root, top_valid, "config", err)) {
      if (!err.message.empty()) {
        err.message += " (in " +
                       std::string(c.source_path.empty() ? "wilfred.yml" : c.source_path) + ")";
      }
      return false;
    }
  }

  if (auto* search = root.get("search")) {
    if (!search->is_map()) {
      err.message = "search: must be a mapping, e.g.\nsearch:\n  max_results: 40";
      return false;
    }
    std::vector<std::string> valid = {"include_system_files",
                                      "include_hidden_files",
                                      "show_system_in_results",
                                      "max_results",
                                      "debounce_ms",
                                      "web_search_fallback",
                                      "treat_urls_as_open",
                                      "min_query_length",
                                      "fuzzy",
                                      "acronyms",
                                      "context_aware",
                                      "clipboard",
                                      "minis",
                                      "macros",
                                      "snippets",
                                      "plugins"};
    if (!check_unknown_keys(*search, valid, "search", err)) return false;
    for (auto* k : {"include_system_files", "include_hidden_files", "show_system_in_results",
                    "web_search_fallback", "treat_urls_as_open", "fuzzy", "acronyms",
                    "context_aware", "clipboard", "minis", "macros", "snippets", "plugins"})
      if (!expect_bool(search, k, "search", err)) return false;
    for (auto* k : {"max_results", "debounce_ms", "min_query_length"})
      if (!expect_int(search, k, "search", err)) return false;
    c.search.include_system_files = search->boolean("include_system_files", false);
    c.search.include_hidden_files = search->boolean("include_hidden_files", true);
    c.search.show_system_in_results = search->boolean("show_system_in_results", false);
    c.search.max_results = static_cast<int>(search->integer("max_results", 40));
    c.search.debounce_ms = static_cast<int>(search->integer("debounce_ms", 12));
    c.search.web_search_fallback = search->boolean("web_search_fallback", true);
    c.search.treat_urls_as_open = search->boolean("treat_urls_as_open", true);
    c.search.min_query_length = static_cast<int>(search->integer("min_query_length", 1));
    c.search.fuzzy = search->boolean("fuzzy", true);
    c.search.acronyms = search->boolean("acronyms", true);
    c.search.context_aware = search->boolean("context_aware", true);
    c.search.clipboard = search->boolean("clipboard", true);
    c.search.minis = search->boolean("minis", true);
    c.search.macros = search->boolean("macros", true);
    c.search.snippets = search->boolean("snippets", true);
    c.search.plugins = search->boolean("plugins", true);
    if (c.search.max_results < 1 || c.search.max_results > 500) {
      err.message = "search.max_results must be between 1 and 500";
      return false;
    }
    if (c.search.debounce_ms < 0 || c.search.debounce_ms > 2000) {
      err.message = "search.debounce_ms must be between 0 and 2000";
      return false;
    }
  }

  if (auto* index = root.get("index")) {
    if (!index->is_map()) {
      err.message = "index: must be a mapping, e.g.\nindex:\n  paths: []\n  exclude: [node_modules]";
      return false;
    }
    std::vector<std::string> valid = {"paths",
                                      "exclude",
                                      "exclude_globs",
                                      "system_directories",
                                      "follow_symlinks",
                                      "index_hidden",
                                      "index_system",
                                      "usn_scan",
                                      "max_file_size_bytes",
                                      "content_indexing",
                                      "content_max_bytes",
                                      "content_max_tokens",
                                      "workers",
                                      "cpu_percent_limit",
                                      "memory_limit_mb",
                                      "batch_size",
                                      "debounce_fs_ms",
                                      "rescan_interval_seconds",
                                      "persist_every_records",
                                      "wal_compact_bytes",
                                      "extensions"};
    if (!check_unknown_keys(*index, valid, "index", err)) return false;
    for (auto* k : {"follow_symlinks", "index_hidden", "index_system", "usn_scan",
                    "content_indexing"})
      if (!expect_bool(index, k, "index", err)) return false;
    for (auto* k : {"max_file_size_bytes", "content_max_bytes", "content_max_tokens", "workers",
                    "cpu_percent_limit", "memory_limit_mb", "batch_size", "debounce_fs_ms",
                    "rescan_interval_seconds", "persist_every_records", "wal_compact_bytes"})
      if (!expect_int(index, k, "index", err)) return false;
    c.index.paths = index->string_list("paths");
    c.index.exclude = index->string_list("exclude");
    c.index.exclude_globs = index->string_list("exclude_globs");
    c.index.system_directories = index->string_list("system_directories");
    c.index.follow_symlinks = index->boolean("follow_symlinks", false);
    c.index.index_hidden = index->boolean("index_hidden", true);
    c.index.index_system = index->boolean("index_system", false);
    c.index.usn_scan = index->boolean("usn_scan", true);
    c.index.max_file_size_bytes =
        static_cast<std::uint64_t>(std::max<std::int64_t>(0, index->integer("max_file_size_bytes", 0)));
    c.index.content_indexing = index->boolean("content_indexing", true);
    c.index.content_max_bytes =
        static_cast<std::uint64_t>(index->integer("content_max_bytes", 131072));
    c.index.content_max_tokens = static_cast<int>(index->integer("content_max_tokens", 480));
    c.index.workers = static_cast<int>(index->integer("workers", 0));
    c.index.cpu_percent_limit = static_cast<int>(index->integer("cpu_percent_limit", 45));
    c.index.memory_limit_mb = static_cast<int>(index->integer("memory_limit_mb", 384));
    c.index.batch_size = static_cast<int>(index->integer("batch_size", 2048));
    c.index.debounce_fs_ms = static_cast<int>(index->integer("debounce_fs_ms", 80));
    c.index.rescan_interval_seconds =
        static_cast<int>(index->integer("rescan_interval_seconds", 0));
    c.index.persist_every_records =
        static_cast<int>(index->integer("persist_every_records", 50000));
    c.index.wal_compact_bytes =
        static_cast<std::uint64_t>(index->integer("wal_compact_bytes", 8 * 1024 * 1024));
    if (auto* ext = index->get("extensions")) {
      if (!ext->is_map()) {
        err.message = "index.extensions must be a mapping with include/exclude lists";
        return false;
      }
      c.index.ext_include = ext->string_list("include");
      c.index.ext_exclude = ext->string_list("exclude");
    }
    for (auto& e : c.index.ext_include) e = norm_ext(e);
    for (auto& e : c.index.ext_exclude) e = norm_ext(e);
    if (c.index.cpu_percent_limit < 5 || c.index.cpu_percent_limit > 100) {
      err.message = "index.cpu_percent_limit must be between 5 and 100";
      return false;
    }
    if (c.index.memory_limit_mb < 32) {
      err.message = "index.memory_limit_mb must be at least 32";
      return false;
    }
    if (c.index.workers < 0 || c.index.workers > 256) {
      err.message = "index.workers must be between 0 (auto) and 256";
      return false;
    }
  }

  if (auto* r = root.get("ranking")) {
    if (!r->is_map()) {
      err.message = "ranking: must be a mapping of integer weights";
      return false;
    }
    c.ranking.exact_name = static_cast<int>(r->integer("exact_name", c.ranking.exact_name));
    c.ranking.prefix_name = static_cast<int>(r->integer("prefix_name", c.ranking.prefix_name));
    c.ranking.substring_name =
        static_cast<int>(r->integer("substring_name", c.ranking.substring_name));
    c.ranking.fuzzy_name = static_cast<int>(r->integer("fuzzy_name", c.ranking.fuzzy_name));
    c.ranking.acronym = static_cast<int>(r->integer("acronym", c.ranking.acronym));
    c.ranking.path_component =
        static_cast<int>(r->integer("path_component", c.ranking.path_component));
    c.ranking.extension = static_cast<int>(r->integer("extension", c.ranking.extension));
    c.ranking.application = static_cast<int>(r->integer("application", c.ranking.application));
    c.ranking.recency = static_cast<int>(r->integer("recency", c.ranking.recency));
    c.ranking.frequency = static_cast<int>(r->integer("frequency", c.ranking.frequency));
    c.ranking.previous_selection =
        static_cast<int>(r->integer("previous_selection", c.ranking.previous_selection));
    c.ranking.word_boundary =
        static_cast<int>(r->integer("word_boundary", c.ranking.word_boundary));
    c.ranking.token_proximity =
        static_cast<int>(r->integer("token_proximity", c.ranking.token_proximity));
    c.ranking.directory_bonus =
        static_cast<int>(r->integer("directory_bonus", c.ranking.directory_bonus));
    c.ranking.alias = static_cast<int>(r->integer("alias", c.ranking.alias));
    c.ranking.learned_choice =
        static_cast<int>(r->integer("learned_choice", c.ranking.learned_choice));
    c.ranking.context_parent =
        static_cast<int>(r->integer("context_parent", c.ranking.context_parent));
    c.ranking.context_extension =
        static_cast<int>(r->integer("context_extension", c.ranking.context_extension));
    c.ranking.access_recency =
        static_cast<int>(r->integer("access_recency", c.ranking.access_recency));
    c.ranking.clipboard_overlap =
        static_cast<int>(r->integer("clipboard_overlap", c.ranking.clipboard_overlap));
    c.ranking.content_hit = static_cast<int>(r->integer("content_hit", c.ranking.content_hit));
    c.ranking.hour_affinity =
        static_cast<int>(r->integer("hour_affinity", c.ranking.hour_affinity));
  }

  if (auto* a = root.get("aliases")) {
    if (!a->is_map()) {
      err.message = "aliases: must be a mapping of query -> target";
      return false;
    }
    for (auto& [k, v] : a->as_map()) {
      if (!v.is_string()) {
        err.message = "alias '" + k + "' must be a string";
        return false;
      }
      c.aliases[to_lower_utf8(k)] = v.as_string();
    }
  }

  if (auto* macros = root.get("macros")) {
    if (!macros->is_map()) {
      err.message = "macros: must be a mapping of name -> URL template";
      return false;
    }
    for (auto& [k, v] : macros->as_map()) {
      if (!v.is_string()) {
        err.message = "macro '" + k + "' must be a string";
        return false;
      }
      c.macros[to_lower_utf8(k)] = v.as_string();
    }
  }

  if (auto* sc = root.get("scopes")) {
    if (!sc->is_map()) {
      err.message = "scopes: must be a mapping of name -> path list";
      return false;
    }
    for (auto& [k, v] : sc->as_map()) {
      auto key = to_lower_utf8(k);
      if (v.is_string()) {
        c.scopes[key].push_back(v.as_string());
      } else if (v.is_list()) {
        for (auto& item : v.as_list()) {
          if (!item.is_string()) {
            err.message = "scope '" + k + "' entries must be strings";
            return false;
          }
          c.scopes[key].push_back(item.as_string());
        }
      } else {
        err.message = "scope '" + k + "' must be a string or list of strings";
        return false;
      }
    }
  }

  if (auto* md = root.get("custom_metadata")) {
    if (!md->is_map()) {
      err.message = "custom_metadata: must be a mapping of string -> string";
      return false;
    }
    for (auto& [k, v] : md->as_map()) {
      if (!v.is_string()) {
        err.message = "custom_metadata '" + k + "' must be a string";
        return false;
      }
      c.custom_metadata[k] = v.as_string();
    }
  }

  if (auto* h = root.get("history")) {
    if (!h->is_map()) {
      err.message = "history: must be a mapping";
      return false;
    }
    c.history.enabled = h->boolean("enabled", true);
    c.history.max_entries = static_cast<int>(h->integer("max_entries", 8000));
    c.history.persist = h->boolean("persist", true);
    if (c.history.max_entries < 0) {
      err.message = "history.max_entries must be >= 0";
      return false;
    }
  }

  if (auto* cb = root.get("clipboard")) {
    if (!cb->is_map()) {
      err.message = "clipboard: must be a mapping";
      return false;
    }
    c.clipboard.manager = cb->boolean("manager", true);
    c.clipboard.max_entries = static_cast<int>(cb->integer("max_entries", 200));
    c.clipboard.persist = cb->boolean("persist", true);
    if (c.clipboard.max_entries < 0) {
      err.message = "clipboard.max_entries must be >= 0";
      return false;
    }
  }

  if (auto* hk = root.get("hotkey")) {
    if (!hk->is_map()) {
      err.message = "hotkey: must be a mapping";
      return false;
    }
    c.hotkey.enabled = hk->boolean("enabled", true);
    auto mods = hk->string_list("modifiers");
    if (!mods.empty()) c.hotkey.modifiers = mods;
    auto key = hk->str("key", "W");
    if (key.empty()) {
      err.message = "hotkey.key must be a non-empty key name";
      return false;
    }
    c.hotkey.key = key;
    c.hotkey.use_command_on_macos = hk->boolean("use_command_on_macos", true);
  }

  if (auto* hks = root.get("hotkeys")) {
    if (!hks->is_map()) {
      err.message =
          "hotkeys: must be a mapping of name -> binding, e.g.\nhotkeys:\n  google-clip:\n    modifiers: [ctrl, alt]\n    key: G\n    run: macro:gclip";
      return false;
    }
    for (auto& [k, v] : hks->as_map()) {
      auto where = "hotkeys.'" + k + "'";
      if (!v.is_map()) {
        err.message = where + " must be a mapping with modifiers/key/run";
        return false;
      }
      std::vector<std::string> valid = {"modifiers", "key", "run"};
      if (!check_unknown_keys(v, valid, where, err)) return false;
      Config::HotkeyBinding b;
      b.name = to_lower_utf8(k);
      b.modifiers = v.string_list("modifiers");
      if (b.modifiers.size() == 1 && !b.modifiers[0].empty() && b.modifiers[0].front() == '[')
        b.modifiers = split_flow_or_plus(b.modifiers[0]);  // inline [ctrl, alt] form
      if (b.modifiers.empty()) b.modifiers = {"ctrl", "alt"};
      b.key = v.str("key", "");
      if (b.key.empty()) {
        err.message = where + " needs a non-empty key name (e.g. key: G)";
        return false;
      }
      b.run = v.str("run", "show");
      auto rl = to_lower_utf8(b.run);
      bool ok_run = rl == "show" || rl.rfind("macro:", 0) == 0 || rl.rfind("system:", 0) == 0 ||
                    rl.rfind("media:", 0) == 0 || rl.rfind("workflow:", 0) == 0;
      if (!ok_run) {
        err.message = where + " run must be show, macro:<text>, system:<id>, media:<id>, or " +
                      "workflow:<name>; got '" + b.run + "'";
        return false;
      }
      c.hotkeys.push_back(std::move(b));
      if (c.hotkeys.size() > 16) {
        err.message = "hotkeys: at most 16 extra bindings";
        return false;
      }
    }
  }

  if (auto* b = root.get("browser")) {
    if (!b->is_map()) {
      err.message = "browser: must be a mapping";
      return false;
    }
    c.browser.provider = b->str("provider", "auto");
    c.browser.search_template =
        b->str("search_template", "https://www.google.com/search?q={query}");
    c.browser.library = b->boolean("library", true);
    if (c.browser.search_template.find("{query}") == std::string::npos) {
      err.message = "browser.search_template must contain {query}";
      return false;
    }
  }

  if (auto* l = root.get("logging")) {
    if (!l->is_map()) {
      err.message = "logging: must be a mapping";
      return false;
    }
    c.logging.level = l->str("level", "info");
    if (!parse_level_ok(c.logging.level)) {
      err.message = "logging.level must be error, warn, info, or debug";
      return false;
    }
    c.logging.file = l->str("file", "");
    c.logging.max_file_bytes =
        static_cast<std::uint64_t>(l->integer("max_file_bytes", 2 * 1024 * 1024));
  }

  if (auto* ui = root.get("ui")) {
    if (!ui->is_map()) {
      err.message = "ui: must be a mapping";
      return false;
    }
    c.ui.theme = ui->str("theme", "dark");
    c.ui.max_visible = static_cast<int>(ui->integer("max_visible", 9));
    c.ui.width = static_cast<int>(ui->integer("width", 720));
  }

  if (auto* pl = root.get("plugins")) {
    if (!pl->is_map()) {
      err.message = "plugins: must be a mapping";
      return false;
    }
    std::vector<std::string> valid = {"enabled", "directories", "timeout_ms", "registry",
                                      "require_approval"};
    if (!check_unknown_keys(*pl, valid, "plugins", err)) return false;
    if (!expect_bool(pl, "enabled", "plugins", err) ||
        !expect_bool(pl, "require_approval", "plugins", err))
      return false;
    if (!expect_int(pl, "timeout_ms", "plugins", err)) return false;
    c.plugins.enabled = pl->boolean("enabled", true);
    c.plugins.directories = pl->string_list("directories");
    c.plugins.timeout_ms = static_cast<int>(pl->integer("timeout_ms", 400));
    if (c.plugins.timeout_ms < 50 || c.plugins.timeout_ms > 30000) {
      err.message = "plugins.timeout_ms must be between 50 and 30000";
      return false;
    }
    c.plugins.registry = pl->str("registry", "");
    if (!c.plugins.registry.empty() &&
        c.plugins.registry.rfind("http://", 0) != 0 &&
        c.plugins.registry.rfind("https://", 0) != 0) {
      err.message = "plugins.registry must start with http:// or https://";
      return false;
    }
    c.plugins.require_approval = pl->boolean("require_approval", true);
  }

  if (auto* pr = root.get("providers")) {
    if (!pr->is_map()) {
      err.message = "providers: must be a mapping";
      return false;
    }
    c.providers.semantic = pr->boolean("semantic", false);
    c.providers.semantic_min_score = pr->number("semantic_min_score", 0.3);
    if (c.providers.semantic_min_score < 0 || c.providers.semantic_min_score > 1) {
      err.message = "providers.semantic_min_score must be between 0 and 1";
      return false;
    }
    c.providers.semantic_backend = pr->str("semantic_backend", "hybrid");
    c.providers.semantic_max_results =
        static_cast<int>(pr->integer("semantic_max_results", 10));
    if (c.providers.semantic_max_results < 1 || c.providers.semantic_max_results > 100) {
      err.message = "providers.semantic_max_results must be between 1 and 100";
      return false;
    }
  }

  if (auto* em = root.get("embedding")) {
    if (!em->is_map()) {
      err.message = "embedding: must be a mapping";
      return false;
    }
    c.embedding.enabled = em->boolean("enabled", false);
    c.embedding.backend = em->str("backend", "auto");
    c.embedding.model = em->str("model", "");
    c.embedding.endpoint = em->str("endpoint", "");
    c.embedding.dim = static_cast<int>(em->integer("dim", 384));
    if (c.embedding.dim < 32 || c.embedding.dim > 4096) {
      err.message = "embedding.dim must be between 32 and 4096";
      return false;
    }
    c.embedding.min_score = em->number("min_score", 0.45);
    if (c.embedding.min_score < 0 || c.embedding.min_score > 1) {
      err.message = "embedding.min_score must be between 0 and 1";
      return false;
    }
    c.embedding.max_results = static_cast<int>(em->integer("max_results", 10));
    if (c.embedding.max_results < 1 || c.embedding.max_results > 100) {
      err.message = "embedding.max_results must be between 1 and 100";
      return false;
    }
  }

  if (auto* ai = root.get("ai")) {
    if (!ai->is_map()) {
      err.message = "ai: must be a mapping";
      return false;
    }
    c.ai.enabled = ai->boolean("enabled", false);
    c.ai.provider = ai->str("provider", "auto");
    c.ai.model = ai->str("model", "");
    c.ai.api_key = ai->str("api_key", ai->str("apiKey", ""));
    c.ai.endpoint = ai->str("endpoint", "");
    c.ai.max_tokens = static_cast<int>(ai->integer("max_tokens", 1024));
    if (c.ai.max_tokens < 1 || c.ai.max_tokens > 128000) {
      err.message = "ai.max_tokens must be between 1 and 128000";
      return false;
    }
    c.ai.temperature = ai->number("temperature", 0.7);
    if (c.ai.temperature < 0 || c.ai.temperature > 2) {
      err.message = "ai.temperature must be between 0 and 2";
      return false;
    }
    c.ai.timeout_ms = static_cast<int>(ai->integer("timeout_ms", 30000));
    if (c.ai.timeout_ms < 1000 || c.ai.timeout_ms > 300000) {
      err.message = "ai.timeout_ms must be between 1000 and 300000";
      return false;
    }
  }

  if (auto* so = root.get("sources")) {
    if (!so->is_map()) {
      err.message = "sources: must be a mapping";
      return false;
    }
    c.sources.calendar = so->boolean("calendar", true);
    c.sources.contacts = so->boolean("contacts", true);
    c.sources.notes = so->boolean("notes", true);
    c.sources.calendar_paths = so->string_list("calendar_paths");
    c.sources.contacts_paths = so->string_list("contacts_paths");
    c.sources.notes_paths = so->string_list("notes_paths");
    c.sources.ocr = so->boolean("ocr", false);
    c.sources.ocr_languages = so->str("ocr_languages", "eng");
    c.sources.max_results = static_cast<int>(so->integer("max_results", 8));
    if (c.sources.max_results < 1 || c.sources.max_results > 50) {
      err.message = "sources.max_results must be between 1 and 50";
      return false;
    }
  }

  if (auto* rm = root.get("remotes")) {
    if (!rm->is_map()) {
      err.message =
          "remotes: must be a mapping, e.g.\nremotes:\n  enabled: true\n  sources:\n    - name: wiki\n      url: \"https://example.com/search?q={query_enc}\"";
      return false;
    }
    std::vector<std::string> valid = {"enabled", "timeout_ms", "max_results", "sources"};
    if (!check_unknown_keys(*rm, valid, "remotes", err)) return false;
    if (!expect_bool(rm, "enabled", "remotes", err)) return false;
    for (auto* k : {"timeout_ms", "max_results"})
      if (!expect_int(rm, k, "remotes", err)) return false;
    c.remotes.enabled = rm->boolean("enabled", false);
    c.remotes.timeout_ms = static_cast<int>(rm->integer("timeout_ms", 5000));
    c.remotes.max_results = static_cast<int>(rm->integer("max_results", 8));
    if (c.remotes.timeout_ms < 1000 || c.remotes.timeout_ms > 30000) {
      err.message = "remotes.timeout_ms must be between 1000 and 30000";
      return false;
    }
    if (c.remotes.max_results < 1 || c.remotes.max_results > 50) {
      err.message = "remotes.max_results must be between 1 and 50";
      return false;
    }
    if (auto* srcs = rm->get("sources")) {
      auto push_source = [&](Config::RemoteSourceCfg s) -> bool {
        if (s.url.empty()) {
          err.message = "remotes.sources entries need a non-empty url";
          return false;
        }
        if (s.url.rfind("http://", 0) != 0 && s.url.rfind("https://", 0) != 0) {
          err.message = "remotes.sources url must start with http:// or https:// (got '" + s.url +
                        "')";
          return false;
        }
        if (s.url.size() > 2048) {
          err.message = "remotes.sources url is too long (max 2048)";
          return false;
        }
        if (s.max_results < 0 || s.max_results > 50) {
          err.message = "remotes.sources '" + s.name + "' max_results must be between 0 and 50";
          return false;
        }
        if (s.headers.size() > 8) {
          err.message = "remotes.sources '" + s.name + "' supports at most 8 headers";
          return false;
        }
        for (auto& [hk, hv] : s.headers) {
          if (hk.empty() || hk.size() > 64 || hv.size() > 1024) {
            err.message = "remotes.sources '" + s.name + "' has an oversized header";
            return false;
          }
          for (char c : hk) {
            if (c == '\n' || c == '\r' || c == ':') {
              err.message = "remotes.sources '" + s.name + "' has an invalid header name";
              return false;
            }
          }
          for (char c : hv) {
            if (c == '\n' || c == '\r') {
              err.message = "remotes.sources '" + s.name + "' has an invalid header value";
              return false;
            }
          }
        }
        c.remotes.sources.push_back(std::move(s));
        if (c.remotes.sources.size() > 16) {
          err.message = "remotes.sources supports at most 16 sources";
          return false;
        }
        return true;
      };
      if (srcs->is_map()) {
        // Shorthand: sources: {wiki: "https://..."}.
        for (auto& [k, v] : srcs->as_map()) {
          if (!v.is_string() || v.as_string().empty()) {
            err.message = "remotes.sources '" + k + "' must be a non-empty URL string";
            return false;
          }
          Config::RemoteSourceCfg s;
          s.name = k;
          s.url = v.as_string();
          if (!push_source(std::move(s))) return false;
        }
      } else if (srcs->is_list()) {
        for (auto& item : srcs->as_list()) {
          if (!item.is_map()) {
            err.message = "remotes.sources entries must be {name, url} mappings";
            return false;
          }
          Config::RemoteSourceCfg s;
          s.name = item.str("name", "");
          s.url = item.str("url", "");
          s.max_results = static_cast<int>(item.integer("max_results", 0));
          if (auto* hdrs = item.get("headers")) {
            if (!hdrs->is_map()) {
              err.message = "remotes.sources '" + s.name + "' headers must be a mapping";
              return false;
            }
            for (auto& [hk, hv] : hdrs->as_map()) {
              if (!hv.is_string()) {
                err.message = "remotes.sources '" + s.name + "' header '" + hk +
                              "' must be a string";
                return false;
              }
              s.headers[hk] = hv.as_string();
            }
          }
          if (!push_source(std::move(s))) return false;
        }
      } else {
        err.message = "remotes.sources must be a list of {name, url} mappings or a name->url map";
        return false;
      }
    }
  }

  if (auto* pk = root.get("packages")) {
    if (!pk->is_map()) {
      err.message =
          "packages: must be a mapping, e.g.\npackages:\n  enabled: true\n  managers: [winget]";
      return false;
    }
    std::vector<std::string> valid = {"enabled", "max_results", "timeout_ms", "managers"};
    if (!check_unknown_keys(*pk, valid, "packages", err)) return false;
    if (!expect_bool(pk, "enabled", "packages", err)) return false;
    for (auto* k : {"max_results", "timeout_ms"})
      if (!expect_int(pk, k, "packages", err)) return false;
    c.packages.enabled = pk->boolean("enabled", true);
    c.packages.max_results = static_cast<int>(pk->integer("max_results", 8));
    c.packages.timeout_ms = static_cast<int>(pk->integer("timeout_ms", 8000));
    if (c.packages.max_results < 1 || c.packages.max_results > 20) {
      err.message = "packages.max_results must be between 1 and 20";
      return false;
    }
    if (c.packages.timeout_ms < 2000 || c.packages.timeout_ms > 30000) {
      err.message = "packages.timeout_ms must be between 2000 and 30000";
      return false;
    }
    // Must match pkg_known_managers() in search/pkg.hpp.
    static const char* known[] = {"winget", "brew", "apt", "choco", "flatpak", "pacman",
                                  nullptr};
    auto managers = pk->string_list("managers");
    if (managers.size() == 1 && !managers[0].empty() && managers[0].front() == '[')
      managers = split_flow_or_plus(managers[0]);  // inline [brew, apt] form
    for (auto& m : managers) {
      auto l = to_lower_utf8(m);
      bool ok = false;
      for (auto** p = known; *p; ++p)
        if (l == *p) ok = true;
      if (!ok) {
        err.message = "packages.managers has unknown manager '" + m +
                      "' (try: winget, brew, apt, choco, flatpak, pacman)";
        return false;
      }
      c.packages.managers.push_back(l);
    }
  }

  if (auto* lo = root.get("layouts")) {
    if (!lo->is_map()) {
      err.message =
          "layouts: must be a mapping, e.g.\nlayouts:\n  auto_apply: true\n  auto_layout: work\n  monitor_layouts:\n    docked: work";
      return false;
    }
    std::vector<std::string> valid = {"auto_apply", "auto_layout", "monitor_layouts"};
    if (!check_unknown_keys(*lo, valid, "layouts", err)) return false;
    if (!expect_bool(lo, "auto_apply", "layouts", err)) return false;
    c.layouts.auto_apply = lo->boolean("auto_apply", false);
    c.layouts.auto_layout = lo->str("auto_layout", "");
    if (c.layouts.auto_layout.size() > 64) {
      err.message = "layouts.auto_layout name is too long";
      return false;
    }
    if (auto* ml = lo->get("monitor_layouts")) {
      if (!ml->is_map()) {
        err.message = "layouts.monitor_layouts must be a mapping of signature -> layout name";
        return false;
      }
      for (auto& [k, v] : ml->as_map()) {
        if (!v.is_string() || v.as_string().empty()) {
          err.message = "layouts.monitor_layouts '" + k + "' must be a layout name";
          return false;
        }
        if (k.size() > 256 || v.as_string().size() > 64) {
          err.message = "layouts.monitor_layouts entry is too long";
          return false;
        }
        c.layouts.monitor_layouts[k] = v.as_string();
        if (c.layouts.monitor_layouts.size() > 16) {
          err.message = "layouts.monitor_layouts supports at most 16 entries";
          return false;
        }
      }
    }
  }

  if (auto* tr = root.get("transcription")) {
    if (!tr->is_map()) {
      err.message = "transcription: must be a mapping";
      return false;
    }
    std::vector<std::string> valid = {"enabled", "binary", "model",
                                      "language", "save_txt", "mic"};
    if (!check_unknown_keys(*tr, valid, "transcription", err)) return false;
    if (!expect_bool(tr, "enabled", "transcription", err) ||
        !expect_bool(tr, "save_txt", "transcription", err))
      return false;
    c.transcription.enabled = tr->boolean("enabled", true);
    c.transcription.binary = tr->str("binary", "");
    c.transcription.model = tr->str("model", "");
    c.transcription.language = tr->str("language", "auto");
    c.transcription.save_txt = tr->boolean("save_txt", true);
    c.transcription.mic = tr->str("mic", "");
    if (c.transcription.language.size() > 16) {
      err.message = "transcription.language must be a short language tag like en or auto";
      return false;
    }
  }

  if (auto* api = root.get("api")) {
    if (!api->is_map()) {
      err.message = "api: must be a mapping";
      return false;
    }
    c.api.enabled = api->boolean("enabled", false);
    c.api.bind = api->str("bind", "127.0.0.1");
    c.api.port = static_cast<int>(api->integer("port", 17380));
    c.api.token = api->str("token", "");
    if (c.api.port < 1 || c.api.port > 65535) {
      err.message = "api.port must be between 1 and 65535";
      return false;
    }
  }

  if (auto* sy = root.get("sync")) {
    if (!sy->is_map()) {
      err.message = "sync: must be a mapping";
      return false;
    }
    c.sync.enabled = sy->boolean("enabled", false);
    c.sync.url = sy->str("url", "");
    c.sync.token = sy->str("token", "");
    c.sync.interval_seconds = static_cast<int>(sy->integer("interval_seconds", 0));
    c.sync.include_index = sy->boolean("include_index", true);
    if (c.sync.interval_seconds < 0) {
      err.message = "sync.interval_seconds must be >= 0";
      return false;
    }
  }

  if (auto* sn = root.get("snippets")) {
    if (!sn->is_map()) {
      err.message = "snippets: must be a mapping";
      return false;
    }
    if (!expect_bool(sn, "expansion", "snippets", err) ||
        !expect_bool(sn, "auto_paste", "snippets", err) ||
        !expect_bool(sn, "global_expansion", "snippets", err))
      return false;
    c.snippets.expansion = sn->boolean("expansion", true);
    c.snippets.prefix = sn->str("prefix", ";");
    c.snippets.auto_paste = sn->boolean("auto_paste", false);
    c.snippets.global_expansion = sn->boolean("global_expansion", false);
    if (auto* items = sn->get("items")) {
      if (!items->is_map()) {
        err.message = "snippets.items must be a mapping of trigger -> text";
        return false;
      }
      for (auto& [k, v] : items->as_map()) {
        if (v.is_string())
          c.snippets.items[k] = v.as_string();
        else if (v.is_map())
          c.snippets.items[k] = v.str("body", v.str("text", ""));
        else {
          err.message = "snippets.items '" + k + "' must be a string";
          return false;
        }
      }
    } else {
      for (auto& [k, v] : sn->as_map()) {
        if (k == "expansion" || k == "prefix" || k == "auto_paste" || k == "items") continue;
        if (v.is_string()) c.snippets.items[k] = v.as_string();
      }
    }
  }

  if (auto* wf = root.get("workflows")) {
    if (!wf->is_map()) {
      err.message =
          "workflows: must be a mapping of name -> steps, e.g.\nworkflows:\n  review:\n    - copy_path\n    - reveal";
      return false;
    }
    for (auto& [k, v] : wf->as_map()) {
      auto key = to_lower_utf8(k);
      if (key.empty()) {
        err.message = "workflows: names must be non-empty";
        return false;
      }
      std::vector<std::string> steps;
      if (v.is_string()) {
        // "copy_path+reveal" or flow "[copy_path, reveal]" form.
        steps = split_flow_or_plus(v.as_string());
        if (steps.empty()) {
          err.message = "workflows.'" + k + "' must list at least one action, e.g. 'copy_path+reveal'";
          return false;
        }
      } else if (v.is_list()) {
        for (auto& item : v.as_list()) {
          if (!item.is_string() || item.as_string().empty()) {
            err.message = "workflows.'" + k + "' steps must be non-empty strings";
            return false;
          }
          // Block items may themselves be flow strings.
          auto sub = split_flow_or_plus(item.as_string());
          if (sub.empty()) sub.push_back(item.as_string());
          steps.insert(steps.end(), sub.begin(), sub.end());
        }
        if (steps.empty()) {
          err.message = "workflows.'" + k + "' must list at least one action";
          return false;
        }
      } else if (v.is_map()) {
        // { steps: [...] } form, optionally with description.
        auto* steps_v = v.get("steps");
        if (!steps_v) {
          err.message = "workflows.'" + k + "' must have a 'steps' list, e.g.\nworkflows:\n  " + k +
                        ":\n    steps: [copy_path, reveal]";
          return false;
        }
        if (steps_v->is_string()) {
          steps = split_flow_or_plus(steps_v->as_string());
        } else if (steps_v->is_list()) {
          for (auto& item : steps_v->as_list()) {
            if (!item.is_string() || item.as_string().empty()) {
              err.message = "workflows.'" + k + "' steps must be non-empty strings";
              return false;
            }
            auto sub = split_flow_or_plus(item.as_string());
            if (sub.empty()) sub.push_back(item.as_string());
            // Avoid double-splitting plain ids like "copy_path" (no delimiters → single).
            if (sub.size() == 1 && sub.front() == item.as_string())
              steps.push_back(item.as_string());
            else
              steps.insert(steps.end(), sub.begin(), sub.end());
          }
        } else {
          err.message = "workflows.'" + k + "' must have a 'steps' list";
          return false;
        }
        if (steps.empty()) {
          err.message = "workflows.'" + k + "' must list at least one action";
          return false;
        }
      } else {
        err.message = "workflows.'" + k +
                      "' must be a list of actions or a '+'-joined string, e.g. 'copy_path+reveal'";
        return false;
      }
      for (auto& st : steps) {
        if (!is_valid_action_step(st)) {
          err.message = "workflows.'" + k + "' has unknown action '" + st +
                        "'. Valid actions include: open, reveal, copy_path, copy_text, hash_file, compress_zip, open_terminal, kill_process, media:play, timer_stop, ...";
          return false;
        }
      }
      c.workflows[key] = std::move(steps);
    }
  }

  if (auto* ql = root.get("quicklinks")) {
    if (!ql->is_map()) {
      err.message =
          "quicklinks: must be a mapping of name -> template, e.g.\nquicklinks:\n  docs: \"https://example.com/{query}\"";
      return false;
    }
    for (auto& [k, v] : ql->as_map()) {
      if (!v.is_string() || v.as_string().empty()) {
        err.message = "quicklinks.'" + k +
                      "' must be a non-empty template string, e.g. \"https://example.com/{query}\"";
        return false;
      }
      auto tmpl = v.as_string();
      if (tmpl.find("{query}") == std::string::npos && tmpl.find("{1}") == std::string::npos &&
          tmpl.find("{*}") == std::string::npos && tmpl.find("{q}") == std::string::npos &&
          tmpl.find("{clipboard}") == std::string::npos) {
        err.message = "quicklinks.'" + k +
                      "' should contain a placeholder like {query}, {1}, {*} or {clipboard}. Got '" +
                      tmpl + "'";
        return false;
      }
      c.quicklinks[to_lower_utf8(k)] = tmpl;
    }
  }

  if (auto* aa = root.get("app_actions")) {
    if (!aa->is_map()) {
      err.message =
          "app_actions: must be a mapping of app-name -> action list, e.g.\napp_actions:\n  code:\n    - copy_path\n    - open_terminal";
      return false;
    }
    for (auto& [k, v] : aa->as_map()) {
      auto key = to_lower_utf8(k);
      std::vector<std::string> ids;
      if (v.is_string()) {
        auto raw = v.as_string();
        ids = split_flow_or_plus(raw);
        if (ids.empty()) {
          err.message = "app_actions.'" + k + "' must list at least one action (got '" + raw + "')";
          return false;
        }
      } else if (v.is_list()) {
        for (auto& item : v.as_list()) {
          if (!item.is_string() || item.as_string().empty()) {
            err.message = "app_actions.'" + k + "' entries must be non-empty action ids";
            return false;
          }
          auto sub = split_flow_or_plus(item.as_string());
          if (sub.empty()) sub.push_back(item.as_string());
          // Plain ids have no delimiters → single entry; avoid double work.
          if (sub.size() == 1 && sub.front() == item.as_string())
            ids.push_back(item.as_string());
          else
            ids.insert(ids.end(), sub.begin(), sub.end());
        }
        if (ids.empty()) {
          err.message = "app_actions.'" + k + "' must list at least one action";
          return false;
        }
      } else {
        err.message = "app_actions.'" + k + "' must be an action id or list of ids";
        return false;
      }
      for (auto& aid : ids) {
        if (!is_valid_action_step(aid)) {
          err.message = "app_actions.'" + k + "' has unknown action '" + aid +
                        "'. Valid actions include: open, reveal, copy_path, open_terminal, ...";
          return false;
        }
        c.app_actions[key].push_back(aid);
      }
    }
  }

  if (c.index.system_directories.empty())
    c.index.system_directories = default_system_directories();

  out = std::move(c);
  return true;
}

bool load_config_file(const std::string& path, Config& out, ConfigError& err) {
  std::string text;
  if (!read_file_all(path, text)) {
    err.message = "Unable to read configuration file: " + path;
    return false;
  }
  if (!load_config_text(text, out, err)) return false;
  out.source_path = path;
  return true;
}

Config load_or_create_user_config(ConfigError& err) {
  create_directories(config_directory());
  create_directories(data_directory());
  auto path = default_config_path();
  Config cfg;
  if (!file_exists(path)) {
    // Write a comment-rich default if the shipped file is available; otherwise encode schema.
    std::string shipped;
    const char* candidates[] = {"config/wilfred.default.yml",
                                "/usr/share/wilfred/wilfred.default.yml",
                                "/usr/local/share/wilfred/wilfred.default.yml"};
    bool wrote = false;
    for (auto* c : candidates) {
      if (read_file_all(c, shipped)) {
        write_file_atomic(path, shipped.data(), shipped.size());
        wrote = true;
        break;
      }
    }
    if (!wrote) {
      const char* fallback = "search:\n  max_results: 40\nindex:\n  paths: []\n";
      write_file_atomic(path, fallback, std::strlen(fallback));
    }
  }
  if (!load_config_file(path, cfg, err)) {
    // Keep process alive with defaults, but surface the error.
    Config d;
    d.source_path = path;
    d.index.system_directories = default_system_directories();
    return d;
  }
  return cfg;
}

bool path_is_excluded(const Config& cfg, const std::string& path, const std::string& name) {
  auto lower_name = to_lower_utf8(name);
  for (auto& ex : cfg.index.exclude) {
    auto el = to_lower_utf8(ex);
    if (lower_name == el) return true;
    if (path.find(ex) != std::string::npos) {
      // component match
      if (path_is_under(path, ex) || path_filename(path) == ex) return true;
      // match as directory component
      auto p = path;
      auto pos = p.find(ex);
      while (pos != std::string::npos) {
        bool left = pos == 0 || p[pos - 1] == '/' || p[pos - 1] == '\\';
        auto end = pos + ex.size();
        bool right = end == p.size() || p[end] == '/' || p[end] == '\\';
        if (left && right) return true;
        pos = p.find(ex, pos + 1);
      }
    }
  }
  for (auto& g : cfg.index.exclude_globs) {
    if (glob_match(name, g) || glob_match(path, g)) return true;
  }
  return false;
}

bool extension_allowed(const Config& cfg, const std::string& ext) {
  auto e = norm_ext(ext);
  for (auto& x : cfg.index.ext_exclude)
    if (x == e) return false;
  if (cfg.index.ext_include.empty()) return true;
  if (e.empty()) return true;
  for (auto& x : cfg.index.ext_include)
    if (x == e) return true;
  return false;
}

bool is_system_path(const Config& cfg, const std::string& path) {
  for (auto& s : cfg.index.system_directories) {
    if (path_is_under(path, s)) return true;
  }
  return false;
}

}  // namespace wilfred
