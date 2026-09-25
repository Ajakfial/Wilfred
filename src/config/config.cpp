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

  if (auto* search = root.get("search")) {
    if (!search->is_map()) {
      err.message = "search: must be a mapping";
      return false;
    }
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
      err.message = "index: must be a mapping";
      return false;
    }
    c.index.paths = index->string_list("paths");
    c.index.exclude = index->string_list("exclude");
    c.index.exclude_globs = index->string_list("exclude_globs");
    c.index.system_directories = index->string_list("system_directories");
    c.index.follow_symlinks = index->boolean("follow_symlinks", false);
    c.index.index_hidden = index->boolean("index_hidden", true);
    c.index.index_system = index->boolean("index_system", false);
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

  if (auto* b = root.get("browser")) {
    if (!b->is_map()) {
      err.message = "browser: must be a mapping";
      return false;
    }
    c.browser.provider = b->str("provider", "auto");
    c.browser.search_template =
        b->str("search_template", "https://www.google.com/search?q={query}");
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
