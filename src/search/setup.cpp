#include "wilfred/search/setup.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/native.hpp"

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

}  // namespace wilfred
