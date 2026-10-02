#include "wilfred/import/import.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <sstream>

namespace wilfred::import {
namespace fs = std::filesystem;

namespace {

std::string trim(const std::string& s) {
  std::size_t a = 0;
  while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a;
  std::size_t b = s.size();
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n'))
    --b;
  return s.substr(a, b - a);
}

std::string lower(const std::string& s) { return to_lower_utf8(s); }

bool eq_ci(const std::string& a, const std::string& b) { return lower(a) == lower(b); }

std::string xml_unescape(std::string s) {
  auto rep = [&](const char* from, const char* to) {
    std::string f(from), t(to);
    std::size_t p = 0;
    while ((p = s.find(f, p)) != std::string::npos) {
      s.replace(p, f.size(), t);
      p += t.size();
    }
  };
  rep("&amp;", "&");
  rep("&lt;", "<");
  rep("&gt;", ">");
  rep("&quot;", "\"");
  rep("&apos;", "'");
  return s;
}

std::string percent_decode_lite(const std::string& s) {
  // Only used to normalize already-decoded templates; keep as-is.
  return s;
}

// ---------------------------------------------------------------- YAML emit

std::string yaml_escape_inner(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else if (c == '\t')
      o += "\\t";
    else
      o.push_back(c);
  }
  return o;
}

std::string yq(const std::string& s) { return "\"" + yaml_escape_inner(s) + "\""; }

std::string ykey(const std::string& s) {
  if (s.empty()) return "\"\"";
  bool simple = true;
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.';
    if (!ok) {
      simple = false;
      break;
    }
  }
  if (simple && !(s[0] >= '0' && s[0] <= '9')) return s;
  return yq(s);
}

void emit_str_list(std::ostringstream& os, const char* key, const std::vector<std::string>& v,
                   int indent) {
  std::string pad(static_cast<std::size_t>(indent), ' ');
  os << pad << key << ":";
  if (v.empty()) {
    os << "\n"; // bare empty (no flow []);
    return;
  }
  os << "\n";
  for (auto& s : v) os << pad << "  - " << yq(s) << "\n";
}

void emit_map(std::ostringstream& os, const char* key, const std::map<std::string, std::string>& m,
              int indent) {
  std::string pad(static_cast<std::size_t>(indent), ' ');
  os << pad << key << ":";
  if (m.empty()) {
    os << "\n"; // bare empty (no flow {});
    return;
  }
  os << "\n";
  for (auto& [k, v] : m) os << pad << "  " << ykey(k) << ": " << yq(v) << "\n";
}

void emit_map_unordered(std::ostringstream& os, const char* key,
                        const std::unordered_map<std::string, std::string>& m, int indent) {
  std::map<std::string, std::string> sorted(m.begin(), m.end());
  emit_map(os, key, sorted, indent);
}

// ---------------------------------------------------------------- generic scans

// Find quoted JSON string value for a (case-insensitive) key. Returns true on hit.
bool json_string_for_key(const std::string& text, const std::string& key, std::string& out) {
  std::string tl = lower(text);
  std::string kl = "\"" + lower(key) + "\"";
  std::size_t pos = 0;
  while ((pos = tl.find(kl, pos)) != std::string::npos) {
    std::size_t c = pos + kl.size();
    while (c < text.size() && (text[c] == ' ' || text[c] == '\t' || text[c] == '\r' ||
                              text[c] == '\n'))
      ++c;
    if (c >= text.size() || text[c] != ':') {
      pos += kl.size();
      continue;
    }
    ++c;
    while (c < text.size() && (text[c] == ' ' || text[c] == '\t')) ++c;
    if (c < text.size() && text[c] == '"') {
      ++c;
      std::string v;
      while (c < text.size() && text[c] != '"') {
        if (text[c] == '\\' && c + 1 < text.size()) {
          char n = text[c + 1];
          if (n == 'n')
            v.push_back('\n');
          else if (n == 't')
            v.push_back('\t');
          else if (n == 'r')
            v.push_back('\r');
          else
            v.push_back(n);
          c += 2;
        } else {
          v.push_back(text[c++]);
        }
      }
      out = v;
      return true;
    }
    pos += kl.size();
  }
  return false;
}

// Collect every http(s) URL in text (stops at quote/bracket/space).
std::vector<std::string> find_all_urls(const std::string& text) {
  std::vector<std::string> out;
  std::size_t pos = 0;
  while (pos < text.size()) {
    std::size_t h = text.find("http", pos);
    if (h == std::string::npos) break;
    if (text.compare(h, 7, "http://") != 0 && text.compare(h, 8, "https://") != 0) {
      pos = h + 4;
      continue;
    }
    std::size_t e = h;
    while (e < text.size()) {
      char c = text[e];
      if (c == '"' || c == '\'' || c == '<' || c == '>' || c == ' ' || c == '\t' ||
          c == '\r' || c == '\n' || c == ',' || c == ']')
        break;
      // Keep '}' only if it is clearly not a placeholder tail? URLs with
      // {query} contain braces; don't stop there.
      if (c == '}' && text.compare(e, 2, "}\"") != 0) {
        // Allow braces inside placeholders: only stop on bare } followed by
        // quote/comma/bracket.
        // Peek: if preceding char is part of {query} style, keep going.
        // Simplest: keep braces, they are part of template syntax.
      }
      if (c == '\\' && e + 1 < text.size() && text[e + 1] == 'n') break;
      ++e;
    }
    std::string u = text.substr(h, e - h);
    // Trim trailing punctuation unlikely to be part of URL.
    while (!u.empty() && (u.back() == '.' || u.back() == ';' || u.back() == ')' || u.back() == '}')) {
      // Don't strip a trailing } that closes a {query} placeholder.
      if (u.size() >= 7 && u.compare(u.size() - 7, 7, "{query}") == 0) break;
      if (u.size() >= 3 && u.compare(u.size() - 3, 3, "{q}") == 0) break;
      u.pop_back();
    }
    u = xml_unescape(u);
    if (u.size() > 10) out.push_back(u);
    pos = e;
  }
  // Deduplicate preserving order.
  std::vector<std::string> uniq;
  for (auto& u : out)
    if (std::find(uniq.begin(), uniq.end(), u) == uniq.end()) uniq.push_back(u);
  return out;
}

// Try to find a human name near a URL occurrence (JSON "name"/"title" before it).
std::string guess_name_near(const std::string& text, std::size_t url_pos) {
  std::size_t win_start = url_pos > 800 ? url_pos - 800 : 0;
  std::string window = text.substr(win_start, url_pos - win_start);
  std::string name;
  // Search backwards for the last name-like key.
  const char* keys[] = {"name", "title", "label", "text", "description", nullptr};
  std::size_t best = std::string::npos;
  for (auto** k = keys; *k; ++k) {
    std::string kk = std::string("\"") + *k + "\"";
    std::string wl = lower(window);
    std::size_t p = wl.rfind(lower(kk));
    if (p != std::string::npos && (best == std::string::npos || p > best)) {
      best = p;
      // Extract quoted value after colon.
      std::size_t c = p + kk.size();
      while (c < window.size() && window[c] != ':') ++c;
      if (c < window.size()) {
        ++c;
        while (c < window.size() && (window[c] == ' ' || window[c] == '\t')) ++c;
        if (c < window.size() && window[c] == '"') {
          ++c;
          std::string v;
          while (c < window.size() && window[c] != '"') {
            if (window[c] == '\\' && c + 1 < window.size()) {
              v.push_back(window[c + 1]);
              c += 2;
            } else
              v.push_back(window[c++]);
          }
          name = trim(v);
        }
      }
    }
  }
  return name;
}

std::string guess_keyword_near(const std::string& text, std::size_t url_pos) {
  std::size_t win_start = url_pos > 800 ? url_pos - 800 : 0;
  std::string window = text.substr(win_start, url_pos - win_start);
  const char* keys[] = {"keyword",  "trigger",  "actionkeyword", "actionKeyword",
                        "keywordprefix", "prefix", "key", "shortcut", "alias", nullptr};
  std::string wl = lower(window);
  std::size_t best = std::string::npos;
  std::string best_val;
  for (auto** k = keys; *k; ++k) {
    std::string kk = std::string("\"") + lower(*k) + "\"";
    std::size_t p = wl.rfind(kk);
    if (p == std::string::npos) continue;
    std::size_t c = p + kk.size();
    while (c < window.size() && window[c] != ':') ++c;
    if (c >= window.size()) continue;
    ++c;
    while (c < window.size() && (window[c] == ' ' || window[c] == '\t')) ++c;
    if (c < window.size() && window[c] == '"') {
      ++c;
      std::string v;
      while (c < window.size() && window[c] != '"') {
        if (window[c] == '\\' && c + 1 < window.size()) {
          v.push_back(window[c + 1]);
          c += 2;
        } else
          v.push_back(window[c++]);
      }
      v = trim(v);
      if (!v.empty() && (best == std::string::npos || p > best)) {
        best = p;
        best_val = v;
      }
    }
  }
  return trim(best_val);
}

// ---------------------------------------------------------------- INI

using IniFile = std::map<std::string, std::map<std::string, std::string>>;

IniFile parse_ini(const std::string& text) {
  IniFile out;
  std::string section;
  std::istringstream is(text);
  std::string line;
  while (std::getline(is, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::string t = trim(line);
    if (t.empty() || t[0] == ';' || t[0] == '#') continue;
    if (t.front() == '[') {
      auto e = t.find(']');
      if (e != std::string::npos) section = trim(t.substr(1, e - 1));
      continue;
    }
    auto eq = t.find('=');
    if (eq == std::string::npos) {
      auto c = t.find(':');
      if (c == std::string::npos) continue;
      eq = c;
    }
    std::string k = trim(t.substr(0, eq));
    std::string v = trim(t.substr(eq + 1));
    // Strip surrounding quotes.
    if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') ||
                          (v.front() == '\'' && v.back() == '\'')))
      v = v.substr(1, v.size() - 2);
    out[section][lower(k)] = v;
    out[section]["__key_" + lower(k)] = k;  // keep original case if needed
  }
  return out;
}

}  // namespace

std::string slugify_keyword(const std::string& s) {
  std::string t = trim(s);
  std::string o;
  for (char c : t) {
    unsigned char u = static_cast<unsigned char>(c);
    if ((u >= 'a' && u <= 'z') || (u >= '0' && u <= '9'))
      o.push_back(static_cast<char>(u));
    else if (u >= 'A' && u <= 'Z')
      o.push_back(static_cast<char>(u - 'A' + 'a'));
    else if (c == '-' || c == '_' || c == ' ')
      o.push_back('-');
    // drop everything else
  }
  // Collapse dashes, trim.
  std::string c2;
  bool dash = false;
  for (char c : o) {
    if (c == '-') {
      if (!dash && !c2.empty()) {
        c2.push_back('-');
        dash = true;
      }
    } else {
      c2.push_back(c);
      dash = false;
    }
  }
  while (!c2.empty() && c2.back() == '-') c2.pop_back();
  // Keywords with dashes still work as macros, but single tokens are nicer.
  // Remove dashes for macro keys (yt-style); keep readable.
  std::string nos;
  for (char c : c2)
    if (c != '-') nos.push_back(c);
  std::string res = nos.empty() ? c2 : nos;
  if (res.empty()) res = "search";
  if (res.size() > 24) res = res.substr(0, 24);
  return lower(res);
}

bool looks_like_search_url(const std::string& s) {
  if (s.rfind("http://", 0) != 0 && s.rfind("https://", 0) != 0 &&
      s.rfind("http", 0) != 0)
    return false;
  std::string l = lower(s);
  return l.find("%s") != std::string::npos || l.find("%q") != std::string::npos ||
         l.find("{q") != std::string::npos || l.find("{query") != std::string::npos ||
         l.find("{search") != std::string::npos || l.find("{text") != std::string::npos ||
         l.find("{keyword") != std::string::npos || l.find("{@}") != std::string::npos ||
         l.find("\\{@}") != std::string::npos || l.find("$1") != std::string::npos ||
         l.find("%1") != std::string::npos || l.find("{1}") != std::string::npos ||
         l.find("{*}") != std::string::npos || l.find("?q=") != std::string::npos ||
         l.find("?query=") != std::string::npos || l.find("&q=") != std::string::npos ||
         l.find("?search=") != std::string::npos || l.find("search?q") != std::string::npos;
}

std::string normalize_url_template(std::string url) {
  url = xml_unescape(trim(url));
  // KDE: \{@\} or {@}
  auto rep = [&](const std::string& from, const std::string& to) {
    std::size_t p = 0;
    while ((p = url.find(from, p)) != std::string::npos) {
      url.replace(p, from.size(), to);
      p += to.size();
    }
  };
  rep("\\{@}", "{query}");
  rep("{@}", "{query}");
  rep("%s", "{query}");
  rep("%S", "{query}");
  rep("%q", "{query}");
  rep("%Q", "{query}");
  rep("{q}", "{query}");
  rep("{Q}", "{query}");
  rep("{Query}", "{query}");
  rep("{QUERY}", "{query}");
  rep("{searchTerms}", "{query}");
  rep("{SearchTerms}", "{query}");
  rep("{searchterms}", "{query}");
  rep("{search}", "{query}");
  rep("{Search}", "{query}");
  rep("{text}", "{query}");
  rep("{Text}", "{query}");
  rep("{keyword}", "{query}");
  rep("{Keyword}", "{query}");
  rep("{k}", "{query}");
  rep("$*", "{query}");
  // $1 / %1 / {1} are positional in quicklinks; for macros treat single-arg
  // templates as {query} so `!kw args` works.
  if (url.find("{query}") == std::string::npos) {
    if (url.find("$1") != std::string::npos) rep("$1", "{query}");
    if (url.find("%1") != std::string::npos) rep("%1", "{query}");
  }
  (void)0;
  return url;
}

bool parse_hotkey_string(const std::string& raw, ImportHotkey& out) {
  out = ImportHotkey{};
  std::string t = trim(raw);
  if (t.empty()) return false;
  out.raw = t;
  // Normalize GTK-style <Primary>/<Control>/etc and separators.
  std::string s = t;
  auto rep_ci = [&](const std::string& from, const std::string& to) {
    std::string lf = lower(from), ls = lower(s);
    std::size_t p = 0;
    std::string res;
    std::size_t i = 0;
    while (i < s.size()) {
      if (i + from.size() <= s.size() && lower(s.substr(i, from.size())) == lf) {
        res += to;
        i += from.size();
      } else {
        res.push_back(s[i++]);
      }
    }
    s = res;
    (void)p;
  };
  rep_ci("<primary>", "ctrl+");
  rep_ci("<control>", "ctrl+");
  rep_ci("<ctrl>", "ctrl+");
  rep_ci("<alt>", "alt+");
  rep_ci("<shift>", "shift+");
  rep_ci("<super>", "win+");
  rep_ci("<meta>", "win+");
  rep_ci("<cmd>", "cmd+");
  // Remove remaining angle brackets.
  std::string s2;
  for (char c : s)
    if (c != '<' && c != '>') s2.push_back(c);
  s = s2;
  // Split on +, -, space, comma.
  std::vector<std::string> toks;
  std::string cur;
  for (char c : s) {
    if (c == '+' || c == '-' || c == ' ' || c == ',' || c == '_') {
      if (!cur.empty()) {
        toks.push_back(trim(cur));
        cur.clear();
      }
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) toks.push_back(trim(cur));
  if (toks.empty()) return false;
  std::vector<std::string> mods;
  std::string key;
  for (auto& tok : toks) {
    std::string l = lower(tok);
    if (l == "ctrl" || l == "control" || l == "primary")
      mods.push_back("ctrl");
    else if (l == "alt" || l == "option" || l == "opt")
      mods.push_back("alt");
    else if (l == "shift")
      mods.push_back("shift");
    else if (l == "win" || l == "super" || l == "meta" || l == "cmd" || l == "command" ||
             l == "windows")
      mods.push_back(l == "cmd" || l == "command" ? "cmd" : "win");
    else if (!tok.empty())
      key = tok;
  }
  if (key.empty()) return false;
  // Normalize key: single chars uppercased, known names capitalized.
  std::string kl = lower(key);
  if (kl == "space")
    key = "Space";
  else if (kl == "enter" || kl == "return")
    key = "Enter";
  else if (kl == "tab")
    key = "Tab";
  else if (kl == "esc" || kl == "escape")
    key = "Escape";
  else if (key.size() == 1)
    key = std::string(1, static_cast<char>(std::toupper(static_cast<unsigned char>(key[0]))));
  else {
    key = kl;
    if (!key.empty()) key[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
  }
  // Deduplicate modifiers preserving order.
  std::vector<std::string> um;
  for (auto& m : mods)
    if (std::find(um.begin(), um.end(), m) == um.end()) um.push_back(m);
  if (um.empty()) {
    // A bare key with no modifier is a weak global hotkey; still accept but warn.
  }
  out.has = true;
  out.modifiers = um.empty() ? std::vector<std::string>{"ctrl", "alt"} : um;
  // If no modifier was present, keep parsed key but note raw.
  if (um.empty()) out.modifiers = std::vector<std::string>{"ctrl", "alt"};
  out.key = key;
  return true;
}

std::string hotkey_to_string(const ImportHotkey& hk) {
  if (!hk.has) return {};
  std::string o;
  for (std::size_t i = 0; i < hk.modifiers.size(); ++i) {
    if (i) o += "+";
    o += hk.modifiers[i];
  }
  if (!hk.key.empty()) {
    if (!o.empty()) o += "+";
    o += hk.key;
  }
  return o;
}

// ---------------------------------------------------------------- launchers

static std::string env_get(const char* k) {
  const char* v = std::getenv(k);
  return v ? std::string(v) : std::string();
}

std::vector<LauncherInfo> supported_launchers() {
  std::string home = home_directory();
  std::string appdata = env_get("APPDATA");
  if (appdata.empty()) appdata = path_join(home, "AppData/Roaming");
  std::string localdata = env_get("LOCALAPPDATA");
  if (localdata.empty()) localdata = path_join(home, "AppData/Local");
  std::string xdg_config = env_get("XDG_CONFIG_HOME");
  if (xdg_config.empty()) xdg_config = path_join(home, ".config");
  std::string xdg_data = env_get("XDG_DATA_HOME");
  if (xdg_data.empty()) xdg_data = path_join(home, ".local/share");

  std::vector<LauncherInfo> v;
  v.push_back({"alfred", "Alfred", "macOS",
               "Custom web searches, snippets and workflow keywords from "
               "Alfred.alfredpreferences (prefs.plist / info.plist).",
               "plist (XML)",
               {path_join(home, "Library/Application Support/Alfred/Alfred.alfredpreferences"),
                path_join(home, "Library/Preferences/com.runningwithcrayons.Alfred.plist"),
                path_join(xdg_config, "alfred/Alfred.alfredpreferences")}});

  v.push_back({"raycast", "Raycast", "macOS",
               "Quicklinks (and snippets where present) from Raycast JSON exports.",
               "json",
               {path_join(home, "Library/Application Support/com.raycast.macos/quicklinks.json"),
                path_join(home, "Library/Application Support/com.raycast.macos/preferences.json"),
                path_join(xdg_config, "raycast/quicklinks.json")}});

  v.push_back({"powertoys", "PowerToys Run", "Windows",
               "Activation hotkey and custom web-search URLs from PowerToys Run "
               "settings.json.",
               "json",
               {path_join(localdata, "Microsoft/PowerToys/PowerToys Run/settings.json"),
                path_join(appdata, "Microsoft/PowerToys/PowerToys Run/settings.json"),
                path_join(xdg_config, "powertoys/settings.json")}});

  v.push_back({"flowlauncher", "Flow Launcher", "Windows",
               "Hotkey plus WebSearch SearchSources (name/keyword/URL) and custom "
               "query shortcuts from Flow Launcher Settings.json.",
               "json",
               {path_join(appdata, "FlowLauncher/Settings/Settings.json"),
                path_join(xdg_config, "FlowLauncher/Settings/Settings.json")}});

  v.push_back({"wox", "Wox", "Windows",
               "Alias of Flow Launcher: same Settings.json layout (Wox predates Flow).",
               "json",
               {path_join(appdata, "Wox/Settings/Settings.json"),
                path_join(xdg_config, "Wox/Settings/Settings.json")}});

  v.push_back({"keypirinha", "Keypirinha", "Windows",
               "Global hotkey plus WebSearch profiles (keyword + URL) from "
               "Keypirinha.ini / WebSearch.ini.",
               "ini",
               {path_join(appdata, "Keypirinha/User/Keypirinha.ini"),
                path_join(xdg_config, "keypirinha/keypirinha.ini"),
                path_join(home, "Keypirinha/User/Keypirinha.ini")}});

  v.push_back({"listary", "Listary", "Windows",
               "Launcher hotkey plus keyword web searches from Listary "
               "Preferences.json.",
               "json",
               {path_join(appdata, "Listary/UserData/Preferences.json"),
                path_join(xdg_config, "listary/preferences.json")}});

  v.push_back({"ulauncher", "Ulauncher", "Linux",
               "Show-app hotkey, theme and shortcuts.json web shortcuts "
               "(keyword -> URL or command) from Ulauncher.",
               "json",
               {path_join(xdg_config, "ulauncher/shortcuts.json"),
                path_join(xdg_config, "ulauncher/settings.json")}});

  v.push_back({"albert", "Albert", "Linux",
               "Hotkey plus websearch engines (trigger -> URL) from albert.conf "
               "and engines.json.",
               "ini+json",
               {path_join(xdg_config, "albert/albert.conf"),
                path_join(xdg_data, "albert/org.albert.extension.websearch/engines.json"),
                path_join(home, ".local/share/albert/org.albert.extension.websearch/engines.json")}});

  v.push_back({"krunner", "KRunner / KDE Web Shortcuts", "Linux",
               "Custom web shortcuts (keyword -> URL with \\{@\\} placeholder) from "
               "kuriikwsfilterrc / krunnerrc.",
               "ini",
               {path_join(xdg_config, "kuriikwsfilterrc"),
                path_join(xdg_config, "krunnerrc"),
                path_join(home, ".kde/share/config/kuriikwsfilterrc")}});

  v.push_back({"rofi", "Rofi", "Linux",
               "Theme (dark/light) and any embedded search URLs from config.rasi. "
               "Note: the Rofi summon key usually lives in the window-manager "
               "config, not here.",
               "rasi",
               {path_join(xdg_config, "rofi/config.rasi"),
                path_join(home, ".config/rofi/config.rasi")}});

  return v;
}

const LauncherInfo* find_launcher(const std::string& id) {
  static std::vector<LauncherInfo> cache = supported_launchers();
  std::string l = lower(trim(id));
  for (auto& li : cache)
    if (lower(li.id) == l) return &li;
  // Friendly aliases.
  if (l == "flow" || l == "flow-launcher") {
    for (auto& li : cache)
      if (li.id == "flowlauncher") return &li;
  }
  if (l == "powertoys-run" || l == "powertoysrun") {
    for (auto& li : cache)
      if (li.id == "powertoys") return &li;
  }
  if (l == "krunnerc" || l == "kde" || l == "plasma") {
    for (auto& li : cache)
      if (li.id == "krunner") return &li;
  }
  return nullptr;
}

std::vector<DetectedLauncher> detect_launchers() {
  std::vector<DetectedLauncher> out;
  for (auto& li : supported_launchers()) {
    for (auto& c : li.candidates) {
      if (file_exists(c)) {
        DetectedLauncher d;
        d.info = li;
        d.found_path = c;
        std::error_code ec;
        d.is_directory = fs::is_directory(fs::path(c), ec) && !ec;
        out.push_back(std::move(d));
      }
    }
  }
  return out;
}

std::string detect_id_for_path(const std::string& path, const std::string& content) {
  std::string lp = lower(path);
  auto has = [&](const char* s) { return lp.find(s) != std::string::npos; };
  if (has("alfred")) return "alfred";
  if (has("raycast")) return "raycast";
  if (has("powertoys")) return "powertoys";
  if (has("flowlauncher")) return "flowlauncher";
  if (has("/wox") || has("\\wox") || has("wox/") || has("wox\\")) return "wox";
  if (has("keypirinha")) return "keypirinha";
  if (has("listary")) return "listary";
  if (has("ulauncher")) return "ulauncher";
  if (has("albert")) return "albert";
  if (has("krunner") || has("kuriikwsfilterrc")) return "krunner";
  if (has("rofi") || (lp.size() >= 5 && lp.compare(lp.size() - 5, 5, ".rasi") == 0))
    return "rofi";
  // Content sniffing.
  std::string lc = lower(content.substr(0, 4000));
  if (lc.find("alfred") != std::string::npos && lc.find("<plist") != std::string::npos)
    return "alfred";
  if (lc.find("searchsources") != std::string::npos) return "flowlauncher";
  if (lc.find("shortcuts.json") != std::string::npos || lc.find("hotkey-show-app") != std::string::npos)
    return "ulauncher";
  if (lc.find("open_powerlauncher") != std::string::npos ||
      lc.find("powerlauncher") != std::string::npos)
    return "powertoys";
  if (lc.find("kuriikwsfilterrc") != std::string::npos || content.find("{@}") != std::string::npos)
    return "krunner";
  if (lc.find("configuration") != std::string::npos && lc.find("modi") != std::string::npos)
    return "rofi";
  // Extension fallback.
  if (lp.size() >= 5 && lp.compare(lp.size() - 5, 5, ".plist") == 0) return "alfred";
  if (lp.size() >= 4 && lp.compare(lp.size() - 4, 4, ".ini") == 0) {
    // Could be keypirinha/albert/krunner; default to keypirinha generic INI.
    return "keypirinha";
  }
  if (lp.size() >= 5 && lp.compare(lp.size() - 5, 5, ".json") == 0) return "auto";
  return "auto";
}

// ---------------------------------------------------------------- individual parsers

namespace {

// PowerToys: hotkey object + generic URL scan.
ImportedSettings parse_powertoys(const std::string& text, const std::string& hint,
                                 std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "powertoys";
  s.source_name = "PowerToys Run";
  auto powertoys_code_to_key = [](int code) -> std::string {
    if (code == 32) return "Space";
    if (code == 13) return "Enter";
    if (code == 9) return "Tab";
    if (code == 27) return "Escape";
    if (code == 8) return "Backspace";
    if (code == 46) return "Delete";
    if (code == 45) return "Insert";
    if (code == 36) return "Home";
    if (code == 35) return "End";
    if (code == 33) return "PageUp";
    if (code == 34) return "PageDown";
    if (code >= 65 && code <= 90) return std::string(1, static_cast<char>(code));
    if (code >= 48 && code <= 57) return std::string(1, static_cast<char>(code));
    if (code >= 96 && code <= 105) return std::string(1, static_cast<char>('0' + (code - 96)));
    if (code >= 112 && code <= 123) return "F" + std::to_string(code - 111);
    return {};
  };
  // Hotkey: look for open_powerlauncher object with alt/ctrl/shift/win +
  // action_key (older) or key/code (current: {"win":..,"ctrl":..,"alt":..,
  // "shift":..,"code":32,"key":""}).
  {
    std::string tl = lower(text);
    auto p = tl.find("open_powerlauncher");
    if (p != std::string::npos) {
      std::string win = text.substr(p, std::min<std::size_t>(1200, text.size() - p));
      std::string wl = lower(win);
      auto flag = [&](const char* k) {
        auto q = wl.find(k);
        if (q == std::string::npos) return false;
        auto c = win.find(':', q);
        if (c == std::string::npos) return false;
        std::string rest = lower(win.substr(c, 12));
        return rest.find("true") != std::string::npos;
      };
      bool alt = flag("\"alt\""), ctrl = flag("\"ctrl\""), shift = flag("\"shift\""),
           winm = flag("\"win\"");
      std::string key;
      if (!json_string_for_key(win, "action_key", key) &&
          !json_string_for_key(win, "actionkey", key))
        json_string_for_key(win, "key", key);
      key = trim(key);
      if (key.empty()) {
        int code = json_get_int(win, "code", 0);
        if (code == 0) code = json_get_int(win, "keyCode", 0);
        key = powertoys_code_to_key(code);
      }
      if (!key.empty()) {
        ImportHotkey hk;
        std::string raw;
        if (winm) raw += "Win+";
        if (ctrl) raw += "Ctrl+";
        if (alt) raw += "Alt+";
        if (shift) raw += "Shift+";
        raw += key;
        if (parse_hotkey_string(raw, hk)) s.hotkey = hk;
      }
    }
  }
  if (!s.hotkey.has) {
    std::string hk_raw;
    const char* keys[] = {"activation_shortcut", "hotkey",   "shortcut",
                          "toggle_hotkey",       "show_hotkey", nullptr};
    for (auto** k = keys; *k; ++k) {
      if (json_string_for_key(text, *k, hk_raw) && !hk_raw.empty()) {
        ImportHotkey hk;
        if (parse_hotkey_string(hk_raw, hk)) {
          s.hotkey = hk;
          break;
        }
      }
    }
  }
  // Web searches: explicit SearchSources-style arrays first, then generic scan.
  {
    std::string arr = json_extract_array(text, "SearchSources");
    if (arr.empty()) arr = json_extract_array(text, "searchSources");
    if (arr.empty()) arr = json_extract_array(text, "Searches");
    if (!arr.empty()) {
      for (auto& obj : json_object_array(arr)) {
        std::string name = json_get_string(obj, "Name");
        if (name.empty()) name = json_get_string(obj, "name");
        if (name.empty()) name = json_get_string(obj, "Title");
        std::string url = json_get_string(obj, "Url");
        if (url.empty()) url = json_get_string(obj, "url");
        if (url.empty()) url = json_get_string(obj, "Link");
        std::string kw = json_get_string(obj, "ActionKeyword");
        if (kw.empty()) kw = json_get_string(obj, "Keyword");
        if (kw.empty()) kw = json_get_string(obj, "keyword");
        if (!url.empty() && looks_like_search_url(url)) {
          WebSearch w;
          w.name = name.empty() ? kw : name;
          w.keyword = kw.empty() ? slugify_keyword(name) : lower(trim(kw));
          w.url = normalize_url_template(url);
          w.source = "powertoys";
          s.searches.push_back(std::move(w));
        }
      }
    }
  }
  // Generic URL fallback (dedupe against explicit ones).
  for (auto& u : find_all_urls(text)) {
    if (!looks_like_search_url(u)) continue;
    bool dup = false;
    for (auto& e : s.searches)
      if (e.url == normalize_url_template(u)) {
        dup = true;
        break;
      }
    if (dup) continue;
    auto pos = text.find(u);
    std::string nm = pos == std::string::npos ? "" : guess_name_near(text, pos);
    std::string kw = pos == std::string::npos ? "" : guess_keyword_near(text, pos);
    WebSearch w;
    w.name = nm.empty() ? u : nm;
    w.keyword = kw.empty() ? slugify_keyword(nm.empty() ? u : nm) : lower(trim(kw));
    w.url = normalize_url_template(u);
    w.source = "powertoys";
    s.searches.push_back(std::move(w));
  }
  if (s.searches.empty() && !s.hotkey.has)
    warnings.push_back("PowerToys file parsed but no hotkey or web searches found (" + hint +
                       ").");
  (void)hint;
  return s;
}

ImportedSettings parse_flow(const std::string& text, const std::string& launcher_id,
                            std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = launcher_id;
  s.source_name = launcher_id == "wox" ? "Wox" : "Flow Launcher";
  std::string hk_raw;
  if (json_string_for_key(text, "Hotkey", hk_raw) && !hk_raw.empty()) {
    ImportHotkey hk;
    if (parse_hotkey_string(hk_raw, hk))
      s.hotkey = hk;
    else
      warnings.push_back("Could not parse hotkey '" + hk_raw + "'.");
  } else if (json_string_for_key(text, "hotkey", hk_raw) && !hk_raw.empty()) {
    ImportHotkey hk;
    if (parse_hotkey_string(hk_raw, hk)) s.hotkey = hk;
  }
  // WebSearch SearchSources.
  std::string arr = json_extract_array(text, "SearchSources");
  if (arr.empty()) arr = json_extract_array(text, "searchSources");
  if (!arr.empty()) {
    for (auto& obj : json_object_array(arr)) {
      std::string name = json_get_string(obj, "Name");
      if (name.empty()) name = json_get_string(obj, "name");
      std::string url = json_get_string(obj, "Url");
      if (url.empty()) url = json_get_string(obj, "url");
      std::string kw = json_get_string(obj, "ActionKeyword");
      if (kw.empty()) kw = json_get_string(obj, "Keyword");
      if (kw.empty()) kw = json_get_string(obj, "keyword");
      if (kw.empty()) kw = json_get_string(obj, "trigger");
      if (url.empty()) continue;
      WebSearch w;
      w.name = name.empty() ? kw : name;
      if (w.name.empty()) w.name = url;
      w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
      w.url = normalize_url_template(url);
      w.source = s.source_id;
      s.searches.push_back(std::move(w));
    }
  }
  // Default search -> browser template.
  {
    std::string def = json_get_string(text, "DefaultSearch");
    if (def.empty()) def = json_get_string(text, "defaultSearch");
    if (!def.empty()) {
      for (auto& w : s.searches)
        if (eq_ci(w.name, def) || eq_ci(w.keyword, def)) {
          s.default_search_template = w.url;
          break;
        }
    }
  }
  // Custom query shortcuts / plugin hotkeys that carry URLs.
  for (auto& u : find_all_urls(text)) {
    if (!looks_like_search_url(u)) continue;
    std::string n = normalize_url_template(u);
    bool dup = false;
    for (auto& e : s.searches)
      if (e.url == n) {
        dup = true;
        break;
      }
    if (dup) continue;
    auto pos = text.find(u);
    std::string nm = pos == std::string::npos ? "" : guess_name_near(text, pos);
    std::string kw = pos == std::string::npos ? "" : guess_keyword_near(text, pos);
    WebSearch w;
    w.name = nm.empty() ? u : nm;
    w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
    w.url = n;
    w.source = s.source_id;
    s.searches.push_back(std::move(w));
  }
  // Theme: Flow stores "Theme" like "Dark"/"Light".
  {
    std::string th;
    if (json_string_for_key(text, "Theme", th) && !th.empty()) {
      std::string l = lower(th);
      if (l.find("dark") != std::string::npos)
        s.theme = "dark";
      else if (l.find("light") != std::string::npos)
        s.theme = "light";
    }
  }
  return s;
}

ImportedSettings parse_ini_searches(const std::string& text, const std::string& launcher_id,
                                    const std::string& display, std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = launcher_id;
  s.source_name = display;
  IniFile ini = parse_ini(text);
  // Hotkey: any key containing "hotkey".
  for (auto& [section, kv] : ini) {
    for (auto& [k, v] : kv) {
      if (k.rfind("__key_", 0) == 0) continue;
      if (k.find("hotkey") != std::string::npos && !v.empty() && !s.hotkey.has) {
        ImportHotkey hk;
        if (parse_hotkey_string(v, hk)) s.hotkey = hk;
      }
    }
  }
  // Searches: any url-ish value with a placeholder, or KDE Query keys.
  for (auto& [section, kv] : ini) {
    std::string url;
    auto it = kv.find("url");
    if (it != kv.end()) url = it->second;
    if (url.empty()) {
      auto q = kv.find("query");
      if (q != kv.end()) url = q->second;
    }
    if (url.empty()) {
      auto q = kv.find("search_url");
      if (q != kv.end()) url = q->second;
    }
    if (url.empty()) {
      // KDE web shortcuts use keys like "Query" with {@} placeholder.
      for (auto& [k, v] : kv) {
        if (k.rfind("__key_", 0) == 0) continue;
        if ((k == "query" || k.find("query") == 0 || k == "url" || k.find("url") == 0) &&
            v.find("http") != std::string::npos) {
          url = v;
          break;
        }
      }
    }
    if (url.empty()) continue;
    if (url.find("http") == std::string::npos) continue;
    std::string kw;
    for (const char* ck : {"keyword", "trigger", "shortcut", "actionkeyword", "key", "alias",
                           "defaultshortcut", "defaulshortcut"}) {
      auto f = kv.find(ck);
      if (f != kv.end() && !f->second.empty()) {
        kw = f->second;
        break;
      }
    }
    std::string nm = section;
    // Keypirinha sections look like "profile/Google" or "plugin:WebSearch".
    auto slash = nm.find_last_of("/:");
    if (slash != std::string::npos && slash + 1 < nm.size()) {
      std::string tail = trim(nm.substr(slash + 1));
      if (!tail.empty() && !eq_ci(tail, "websearch")) nm = tail;
    }
    if (nm.empty() || nm == "global" || nm == "app" || nm == "gui") nm = kw.empty() ? url : kw;
    WebSearch w;
    w.name = nm.empty() ? url : nm;
    w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
    // KDE escapes {@} as \{@\}; normalize handles both.
    w.url = normalize_url_template(url);
    w.source = launcher_id;
    // Only keep URL-ish entries.
    if (w.url.find("http") == std::string::npos) continue;
    s.searches.push_back(std::move(w));
  }
  // Keypirinha aliases: [alias] sections? e.g. `alias/foo = bar`.
  for (auto& [section, kv] : ini) {
    if (lower(section) == "aliases" || lower(section).find("alias") == 0) {
      for (auto& [k, v] : kv) {
        if (k.rfind("__key_", 0) == 0) continue;
        if (k.empty() || v.empty()) continue;
        // Skip hotkey/url keys already handled.
        if (k.find("hotkey") != std::string::npos) continue;
        if (v.find("http") != std::string::npos) continue;
        AliasEntry a;
        a.from = lower(trim(k));
        a.to = trim(v);
        if (!a.from.empty() && !a.to.empty()) s.aliases.push_back(std::move(a));
      }
    }
  }
  if (launcher_id == "krunner") {
    // krunnerrc hotkey lives under [General] `hotkey`? Already handled.
    // Theme is a KDE global setting; skip.
    (void)warnings;
  }
  return s;
}

ImportedSettings parse_listary(const std::string& text, std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "listary";
  s.source_name = "Listary";
  std::string hk;
  const char* hk_keys[] = {"launcherHotkey", "hotkey",     "showHotkey", "activationHotkey",
                           "toggleHotkey",   "hotKey",     nullptr};
  for (auto** k = hk_keys; *k; ++k) {
    if (json_string_for_key(text, *k, hk) && !hk.empty()) {
      ImportHotkey h;
      if (parse_hotkey_string(hk, h)) {
        s.hotkey = h;
        break;
      }
      hk.clear();
    }
  }
  // Keywords arrays: "keywords" / "customSearches" / "commands".
  for (const char* ak : {"keywords", "customSearches", "searches", "commands", "webSearches"}) {
    std::string arr = json_extract_array(text, ak);
    if (arr.empty()) continue;
    for (auto& obj : json_object_array(arr)) {
      std::string kw = json_get_string(obj, "keyword");
      if (kw.empty()) kw = json_get_string(obj, "key");
      if (kw.empty()) kw = json_get_string(obj, "trigger");
      std::string url = json_get_string(obj, "url");
      if (url.empty()) url = json_get_string(obj, "link");
      if (url.empty()) url = json_get_string(obj, "command");
      std::string name = json_get_string(obj, "title");
      if (name.empty()) name = json_get_string(obj, "name");
      if (url.empty()) continue;
      if (url.find("http") == std::string::npos) {
        // Non-URL command -> quicklink.
        std::string qk = kw.empty() ? slugify_keyword(name.empty() ? url : name) : lower(trim(kw));
        s.quicklinks[qk] = url;
        continue;
      }
      WebSearch w;
      w.name = name.empty() ? (kw.empty() ? url : kw) : name;
      w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
      w.url = normalize_url_template(url);
      w.source = "listary";
      s.searches.push_back(std::move(w));
    }
  }
  // Fallback generic URLs.
  if (s.searches.empty()) {
    for (auto& u : find_all_urls(text)) {
      if (!looks_like_search_url(u)) continue;
      auto pos = text.find(u);
      std::string nm = pos == std::string::npos ? "" : guess_name_near(text, pos);
      std::string kw = pos == std::string::npos ? "" : guess_keyword_near(text, pos);
      WebSearch w;
      w.name = nm.empty() ? u : nm;
      w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
      w.url = normalize_url_template(u);
      w.source = "listary";
      s.searches.push_back(std::move(w));
    }
  }
  (void)warnings;
  return s;
}

// Alfred / generic plist XML.
ImportedSettings parse_plist(const std::string& text, std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "alfred";
  s.source_name = "Alfred";
  if (text.size() >= 6 && text.compare(0, 6, "bplist") == 0) {
    warnings.push_back(
        "Binary plist detected. Alfred stores binary plists; open Alfred Preferences > "
        "Features > Web Search and use 'Export' (or convert with `plutil -convert xml1`) then "
        "re-run `wilfred import --from <xml file>`.");
    return s;
  }
  if (text.find("<plist") == std::string::npos && text.find("<dict>") == std::string::npos) {
    warnings.push_back("File does not look like an Alfred plist (no <plist>/<dict> found).");
    return s;
  }
  // Scan for <key>keyword</key><string>...</string> occurrences.
  std::size_t pos = 0;
  int found = 0;
  while (true) {
    std::size_t kp = text.find("<key>", pos);
    if (kp == std::string::npos) break;
    std::size_t ke = text.find("</key>", kp);
    if (ke == std::string::npos) break;
    std::string key = trim(text.substr(kp + 5, ke - kp - 5));
    std::string kl = lower(key);
    std::size_t ve = text.find("</string>", ke);
    std::size_t vs = text.find("<string>", ke);
    std::string val;
    if (vs != std::string::npos && (ve != std::string::npos) && vs < ve && vs < ke + 400) {
      val = trim(text.substr(vs + 8, ve - vs - 8));
      val = xml_unescape(val);
    }
    if ((kl == "keyword" || kl == "actionkeyword") && !val.empty()) {
      // Look ahead up to 4000 chars for url/link, and behind for title/text.
      std::string window = text.substr(kp, std::min<std::size_t>(4000, text.size() - kp));
      std::string url;
      for (const char* uk : {"<key>url</key>", "<key>link</key>", "<key>searchurl</key>",
                             "<key>alfredworkflowurl</key>"}) {
        auto up = window.find(uk);
        if (up == std::string::npos) continue;
        auto ss = window.find("<string>", up);
        auto se = window.find("</string>", up);
        if (ss != std::string::npos && se != std::string::npos && ss < se && ss < up + 600) {
          url = trim(window.substr(ss + 8, se - ss - 8));
          url = xml_unescape(url);
          break;
        }
      }
      // Title: look in surrounding 2000 chars.
      std::size_t ws = kp > 2000 ? kp - 2000 : 0;
      std::string around = text.substr(ws, std::min<std::size_t>(4000, text.size() - ws));
      std::string title;
      for (const char* tk : {"<key>text</key>", "<key>title</key>", "<key>name</key>"}) {
        auto tp = around.find(tk);
        if (tp == std::string::npos) continue;
        auto ss = around.find("<string>", tp);
        auto se = around.find("</string>", tp);
        if (ss != std::string::npos && se != std::string::npos && ss < se && ss < tp + 600) {
          std::string cand = trim(around.substr(ss + 8, se - ss - 8));
          cand = xml_unescape(cand);
          if (!cand.empty() && cand.find("http") != 0) {
            title = cand;
            break;
          }
        }
      }
      // Snippet vs search: if a nearby <key>snippet</key> exists, treat as snippet.
      std::string snip;
      {
        auto sp = window.find("<key>snippet</key>");
        if (sp != std::string::npos && sp < 2000) {
          auto ss = window.find("<string>", sp);
          auto se = window.find("</string>", sp);
          // Snippets may use <string> with newlines; take raw.
          if (ss != std::string::npos && se != std::string::npos && ss < se) {
            snip = trim(window.substr(ss + 8, se - ss - 8));
            snip = xml_unescape(snip);
          }
        }
      }
      if (!snip.empty()) {
        Snippet sn;
        sn.trigger = lower(trim(val));
        sn.title = title.empty() ? sn.trigger : title;
        sn.body = snip;
        sn.source = "alfred";
        s.snippets.push_back(std::move(sn));
        ++found;
      } else if (!url.empty() && url.find("http") == 0) {
        WebSearch w;
        w.keyword = lower(trim(val));
        w.name = title.empty() ? w.keyword : title;
        w.url = normalize_url_template(url);
        w.source = "alfred";
        s.searches.push_back(std::move(w));
        ++found;
      } else if (url.empty()) {
        // Workflow keyword without URL (runs a script): keep as quicklink stub
        // pointing at the workflow so nothing is silently dropped.
        // We record a warning instead of a bogus URL.
        warnings.push_back("Alfred keyword '" + val +
                           "' has no URL (likely a script workflow); skipped.");
      }
    }
    pos = ke + 6;
    if (pos >= text.size()) break;
  }
  // Generic URL fallback inside plist (custom searches sometimes store URL under
  // different keys).
  if (s.searches.empty()) {
    for (auto& u : find_all_urls(text)) {
      if (!looks_like_search_url(u)) continue;
      // Avoid duplicating what we already found.
      bool dup = false;
      for (auto& e : s.searches)
        if (e.url == normalize_url_template(u)) {
          dup = true;
          break;
        }
      if (dup) continue;
      auto p = text.find(u);
      // Find nearest preceding keyword within 1500 chars.
      std::string kw;
      if (p != std::string::npos) {
        std::size_t ws = p > 1500 ? p - 1500 : 0;
        std::string back = text.substr(ws, p - ws);
        auto kpos = back.rfind("<key>keyword</key>");
        if (kpos != std::string::npos) {
          auto ss = back.find("<string>", kpos);
          auto se = back.find("</string>", kpos);
          if (ss != std::string::npos && se != std::string::npos && ss < se)
            kw = trim(back.substr(ss + 8, se - ss - 8));
        }
      }
      if (kw.empty()) continue;  // Don't invent keywords for stray URLs.
      WebSearch w;
      w.keyword = lower(kw);
      w.name = kw;
      w.url = normalize_url_template(u);
      w.source = "alfred";
      s.searches.push_back(std::move(w));
    }
  }
  if (found == 0 && s.searches.empty() && s.snippets.empty())
    warnings.push_back("No Alfred keywords/URLs found. Expected prefs.plist with custom "
                       "searches or a workflow info.plist.");
  return s;
}

ImportedSettings parse_raycast(const std::string& text, std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "raycast";
  s.source_name = "Raycast";
  std::string t = trim(text);
  std::string arr;
  if (!t.empty() && t.front() == '[') {
    arr = t;
  } else {
    arr = json_extract_array(text, "quicklinks");
    if (arr.empty()) arr = json_extract_array(text, "quickLinks");
    if (arr.empty()) arr = json_extract_array(text, "links");
    if (arr.empty()) arr = json_extract_array(text, "items");
  }
  auto handle_obj = [&](const std::string& obj) {
    std::string name = json_get_string(obj, "name");
    if (name.empty()) name = json_get_string(obj, "title");
    if (name.empty()) name = json_get_string(obj, "label");
    std::string url = json_get_string(obj, "link");
    if (url.empty()) url = json_get_string(obj, "url");
    if (url.empty()) url = json_get_string(obj, "template");
    if (url.empty()) url = json_get_string(obj, "command");
    std::string kw = json_get_string(obj, "keyword");
    if (kw.empty()) kw = json_get_string(obj, "trigger");
    if (kw.empty()) kw = json_get_string(obj, "alias");
    if (kw.empty()) kw = json_get_string(obj, "key");
    if (url.empty() && name.empty()) return;
    if (!url.empty() && url.find("http") == 0) {
      WebSearch w;
      w.name = name.empty() ? (kw.empty() ? url : kw) : name;
      w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
      w.url = normalize_url_template(url);
      w.source = "raycast";
      s.searches.push_back(std::move(w));
    } else if (!url.empty()) {
      std::string qk = kw.empty() ? slugify_keyword(name.empty() ? url : name) : lower(trim(kw));
      std::string tpl = url;
      if (tpl.find("{query}") == std::string::npos && tpl.find("{1}") == std::string::npos &&
          tpl.find("{*}") == std::string::npos)
        tpl = tpl;  // keep static command
      s.quicklinks[qk] = tpl;
    }
    // Raycast snippet export: objects with keyword + text/body.
    std::string snip_kw = json_get_string(obj, "keyword");
    std::string body = json_get_string(obj, "text");
    if (body.empty()) body = json_get_string(obj, "body");
    if (body.empty()) body = json_get_string(obj, "snippet");
    if (!snip_kw.empty() && !body.empty() && url.empty()) {
      Snippet sn;
      sn.trigger = snip_kw;
      sn.title = name.empty() ? snip_kw : name;
      sn.body = body;
      sn.source = "raycast";
      s.snippets.push_back(std::move(sn));
    }
  };
  if (!arr.empty()) {
    for (auto& obj : json_object_array(arr)) handle_obj(obj);
  } else {
    // Single object file?
    if (text.find('{') != std::string::npos && text.find("http") != std::string::npos)
      handle_obj(text);
  }
  // Hotkey best-effort.
  {
    std::string hk;
    if (json_string_for_key(text, "hotkey", hk) && !hk.empty()) {
      ImportHotkey h;
      if (parse_hotkey_string(hk, h)) s.hotkey = h;
    }
  }
  // Theme best-effort.
  {
    std::string th;
    if (json_string_for_key(text, "theme", th)) {
      std::string l = lower(th);
      if (l.find("dark") != std::string::npos)
        s.theme = "dark";
      else if (l.find("light") != std::string::npos)
        s.theme = "light";
    }
  }
  // Fallback: any search-like URLs not in quicklink objects.
  if (s.searches.empty() && s.quicklinks.empty()) {
    for (auto& u : find_all_urls(text)) {
      if (!looks_like_search_url(u)) continue;
      auto pos = text.find(u);
      std::string nm = pos == std::string::npos ? "" : guess_name_near(text, pos);
      WebSearch w;
      w.name = nm.empty() ? u : nm;
      w.keyword = slugify_keyword(w.name);
      w.url = normalize_url_template(u);
      w.source = "raycast";
      s.searches.push_back(std::move(w));
    }
  }
  if (s.searches.empty() && s.quicklinks.empty() && s.snippets.empty())
    warnings.push_back("No Raycast quicklinks found. Export Raycast quicklinks as JSON "
                       "(array of {name, link}) and pass with --from.");
  return s;
}

ImportedSettings parse_ulauncher(const std::string& text, const std::string& hint,
                                 std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "ulauncher";
  s.source_name = "Ulauncher";
  std::string t = trim(text);
  bool looks_shortcuts = hint.find("shortcut") != std::string::npos ||
                         text.find("\"keyword\"") != std::string::npos;
  std::string arr;
  if (!t.empty() && t.front() == '[')
    arr = t;
  else {
    arr = json_extract_array(text, "shortcuts");
    if (arr.empty()) arr = json_extract_array(text, "items");
  }
  if (!arr.empty() || looks_shortcuts) {
    std::string use = arr.empty() ? text : arr;
    std::vector<std::string> objs;
    if (!use.empty() && trim(use).front() == '[')
      objs = json_object_array(use);
    else
      objs = json_object_array("[" + use + "]");
    for (auto& obj : objs) {
      std::string name = json_get_string(obj, "name");
      std::string kw = json_get_string(obj, "keyword");
      if (kw.empty()) kw = json_get_string(obj, "key");
      std::string cmd = json_get_string(obj, "cmd");
      if (cmd.empty()) cmd = json_get_string(obj, "command");
      if (cmd.empty()) cmd = json_get_string(obj, "url");
      if (cmd.empty()) cmd = json_get_string(obj, "value");
      bool def_search = json_get_bool(obj, "is_default_search", false);
      if (name.empty() && kw.empty() && cmd.empty()) continue;
      if (!cmd.empty() && cmd.find("http") != std::string::npos) {
        WebSearch w;
        w.name = name.empty() ? kw : name;
        if (w.name.empty()) w.name = cmd;
        w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
        w.url = normalize_url_template(cmd);
        w.source = "ulauncher";
        s.searches.push_back(std::move(w));
        if (def_search) s.default_search_template = w.url;
      } else if (!cmd.empty()) {
        std::string qk = kw.empty() ? slugify_keyword(name.empty() ? cmd : name) : lower(trim(kw));
        std::string tpl = cmd;
        // Ulauncher uses %s for query arg.
        tpl = normalize_url_template(tpl);
        s.quicklinks[qk] = tpl;
        if (name.empty()) {
        } else {
          AliasEntry a;
          a.from = qk;
          a.to = name;
          // Only keep alias if it looks like an app mapping (short).
          // Skip to avoid noise; quicklink already covers it.
        }
      }
    }
  }
  // settings.json hotkey/theme in the same or sibling file.
  {
    std::string hk;
    if (json_string_for_key(text, "hotkey-show-app", hk) && !hk.empty()) {
      ImportHotkey h;
      if (parse_hotkey_string(hk, h)) s.hotkey = h;
    } else if (json_string_for_key(text, "hotkey", hk) && !hk.empty()) {
      ImportHotkey h;
      if (parse_hotkey_string(hk, h)) s.hotkey = h;
    }
    std::string th;
    if (json_string_for_key(text, "theme-name", th) && !th.empty()) {
      std::string l = lower(th);
      if (l.find("dark") != std::string::npos)
        s.theme = "dark";
      else if (l.find("light") != std::string::npos)
        s.theme = "light";
    }
  }
  if (s.searches.empty() && s.quicklinks.empty() && !s.hotkey.has)
    warnings.push_back("No Ulauncher shortcuts found. Expected shortcuts.json array with "
                       "{name, keyword, cmd}.");
  return s;
}

ImportedSettings parse_albert(const std::string& text, const std::string& hint,
                              std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "albert";
  s.source_name = "Albert";
  if (hint.find("engines.json") != std::string::npos || trim(text).front() == '[' ||
      text.find("\"trigger\"") != std::string::npos) {
    std::string arr = trim(text).front() == '[' ? trim(text) : json_extract_array(text, "engines");
    if (arr.empty()) arr = text;
    for (auto& obj : json_object_array(arr)) {
      std::string name = json_get_string(obj, "name");
      std::string trig = json_get_string(obj, "trigger");
      if (trig.empty()) trig = json_get_string(obj, "keyword");
      std::string url = json_get_string(obj, "url");
      if (url.empty()) url = json_get_string(obj, "link");
      if (url.empty()) continue;
      WebSearch w;
      w.name = name.empty() ? trig : name;
      if (w.name.empty()) w.name = url;
      w.keyword = trig.empty() ? slugify_keyword(w.name) : lower(trim(trig));
      // Albert triggers often include trailing space ("g "); strip.
      w.keyword = trim(w.keyword);
      w.url = normalize_url_template(url);
      w.source = "albert";
      s.searches.push_back(std::move(w));
    }
  } else {
    // Assume albert.conf INI.
    ImportedSettings ini = parse_ini_searches(text, "albert", "Albert", warnings);
    s.searches = std::move(ini.searches);
    s.aliases = std::move(ini.aliases);
    s.hotkey = ini.hotkey;
    // Albert hotkey key is often "hotkey" under [General]; already handled.
    // Theme under [General] `theme`?
    IniFile kv = parse_ini(text);
    auto g = kv.find("General");
    if (g != kv.end()) {
      auto th = g->second.find("theme");
      if (th != g->second.end()) {
        std::string l = lower(th->second);
        if (l.find("dark") != std::string::npos)
          s.theme = "dark";
        else if (l.find("light") != std::string::npos)
          s.theme = "light";
      }
    }
  }
  return s;
}

ImportedSettings parse_rofi(const std::string& text, std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "rofi";
  s.source_name = "Rofi";
  // theme: `theme: "Arc-Dark";` or `@theme "gruvbox-dark"`.
  {
    std::string l = lower(text);
    auto p = l.find("theme");
    if (p != std::string::npos) {
      std::string win = l.substr(p, 200);
      if (win.find("dark") != std::string::npos)
        s.theme = "dark";
      else if (win.find("light") != std::string::npos)
        s.theme = "light";
    }
  }
  for (auto& u : find_all_urls(text)) {
    if (!looks_like_search_url(u)) continue;
    WebSearch w;
    w.name = u;
    w.keyword = slugify_keyword(u);
    w.url = normalize_url_template(u);
    w.source = "rofi";
    s.searches.push_back(std::move(w));
  }
  warnings.push_back(
      "Rofi config holds theme/modi, not web searches: the summon hotkey usually lives in "
      "the window-manager config (i3/sxhkd), so set Wilfred's hotkey manually if needed.");
  return s;
}

ImportedSettings parse_generic_json(const std::string& text, std::vector<std::string>& warnings) {
  ImportedSettings s;
  s.source_id = "auto";
  s.source_name = "Generic JSON";
  // Hotkey best-effort.
  for (const char* k : {"hotkey", "hotkey-show-app", "activation_shortcut", "shortcut",
                        "launcherHotkey", "show_hotkey"}) {
    std::string v;
    if (json_string_for_key(text, k, v) && !v.empty()) {
      ImportHotkey h;
      if (parse_hotkey_string(v, h)) {
        s.hotkey = h;
        break;
      }
    }
  }
  // Arrays of {name,keyword,url/cmd/link}.
  for (const char* ak : {"quicklinks", "shortcuts", "keywords", "searches", "engines",
                         "SearchSources", "items", "commands", "webSearches"}) {
    std::string arr = json_extract_array(text, ak);
    if (arr.empty()) continue;
    for (auto& obj : json_object_array(arr)) {
      std::string name = json_get_string(obj, "name");
      if (name.empty()) name = json_get_string(obj, "title");
      std::string kw = json_get_string(obj, "keyword");
      if (kw.empty()) kw = json_get_string(obj, "trigger");
      if (kw.empty()) kw = json_get_string(obj, "key");
      if (kw.empty()) kw = json_get_string(obj, "ActionKeyword");
      std::string url = json_get_string(obj, "url");
      if (url.empty()) url = json_get_string(obj, "Url");
      if (url.empty()) url = json_get_string(obj, "link");
      if (url.empty()) url = json_get_string(obj, "cmd");
      if (url.empty()) url = json_get_string(obj, "command");
      if (url.empty()) continue;
      if (url.find("http") == 0) {
        WebSearch w;
        w.name = name.empty() ? (kw.empty() ? url : kw) : name;
        w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
        w.url = normalize_url_template(url);
        w.source = "auto";
        s.searches.push_back(std::move(w));
      } else if (!kw.empty() || !name.empty()) {
        std::string qk = kw.empty() ? slugify_keyword(name) : lower(trim(kw));
        s.quicklinks[qk] = url;
      }
    }
  }
  if (s.searches.empty()) {
    for (auto& u : find_all_urls(text)) {
      if (!looks_like_search_url(u)) continue;
      auto pos = text.find(u);
      std::string nm = pos == std::string::npos ? "" : guess_name_near(text, pos);
      std::string kw = pos == std::string::npos ? "" : guess_keyword_near(text, pos);
      WebSearch w;
      w.name = nm.empty() ? u : nm;
      w.keyword = kw.empty() ? slugify_keyword(w.name) : lower(trim(kw));
      w.url = normalize_url_template(u);
      w.source = "auto";
      s.searches.push_back(std::move(w));
    }
  }
  // Theme.
  {
    std::string th;
    if (json_string_for_key(text, "theme", th)) {
      std::string l = lower(th);
      if (l.find("dark") != std::string::npos)
        s.theme = "dark";
      else if (l.find("light") != std::string::npos)
        s.theme = "light";
    }
  }
  if (s.searches.empty() && s.quicklinks.empty() && !s.hotkey.has)
    warnings.push_back("Generic JSON scan found nothing importable (no hotkey, URLs, or "
                       "shortcut arrays).");
  return s;
}

ImportedSettings parse_generic_ini(const std::string& text, std::vector<std::string>& warnings) {
  return parse_ini_searches(text, "auto", "Generic INI", warnings);
}

}  // namespace

ImportedSettings parse_text(const std::string& launcher_id, const std::string& text,
                            const std::string& path_hint, std::vector<std::string>& warnings) {
  std::string id = lower(trim(launcher_id));
  if (id.empty()) id = "auto";
  if (id == "powertoys") return parse_powertoys(text, path_hint, warnings);
  if (id == "flowlauncher" || id == "flow" || id == "flow-launcher")
    return parse_flow(text, "flowlauncher", warnings);
  if (id == "wox") return parse_flow(text, "wox", warnings);
  if (id == "keypirinha") return parse_ini_searches(text, "keypirinha", "Keypirinha", warnings);
  if (id == "listary") return parse_listary(text, warnings);
  if (id == "alfred") return parse_plist(text, warnings);
  if (id == "raycast") return parse_raycast(text, warnings);
  if (id == "ulauncher") return parse_ulauncher(text, path_hint, warnings);
  if (id == "albert") return parse_albert(text, path_hint, warnings);
  if (id == "krunner" || id == "kde" || id == "plasma" || id == "krunnerrc")
    return parse_ini_searches(text, "krunner", "KRunner", warnings);
  if (id == "rofi") return parse_rofi(text, warnings);
  // auto: dispatch by content sniffing.
  std::string sniff = detect_id_for_path(path_hint, text);
  if (sniff != "auto") return parse_text(sniff, text, path_hint, warnings);
  // Heuristic: plist vs ini vs json.
  if (text.find("<plist") != std::string::npos || text.find("<dict>") != std::string::npos)
    return parse_plist(text, warnings);
  std::string t = trim(text);
  bool ini_like = text.find('[') != std::string::npos && text.find('=') != std::string::npos &&
                  (t.empty() || (t.front() != '{' && t.front() != '['));
  if (ini_like) return parse_generic_ini(text, warnings);
  return parse_generic_json(text, warnings);
}

ImportedSettings parse_directory(const std::string& launcher_id, const std::string& dir_path,
                                 std::vector<std::string>& warnings) {
  ImportedSettings merged;
  merged.source_id = lower(trim(launcher_id));
  if (merged.source_id.empty()) merged.source_id = "auto";
  const LauncherInfo* li = find_launcher(merged.source_id);
  merged.source_name = li ? li->name : merged.source_id;
  std::error_code ec;
  int files = 0;
  for (auto it = fs::recursive_directory_iterator(fs::path(dir_path), ec);
       it != fs::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) break;
    if (!it->is_regular_file(ec)) continue;
    auto p = it->path();
    auto ext = lower(p.extension().string());
    if (ext != ".plist" && ext != ".json" && ext != ".ini" && ext != ".rasi" &&
        ext != ".conf" && ext != ".rc") {
      // Still allow files without extension inside Alfred bundles? Skip others.
      auto name = lower(p.filename().string());
      if (name != "prefs" && name.find("pref") == std::string::npos &&
          name.find("info") == std::string::npos)
        continue;
    }
    // Skip huge files (>4MB) to avoid runaway imports.
    auto sz = it->file_size(ec);
    if (!ec && sz > 4 * 1024 * 1024) {
      warnings.push_back("Skipped large file: " + p.string());
      continue;
    }
    std::string up = p.string();
    std::string text;
    if (!read_file_all(std::string(up.begin(), up.end()), text)) continue;
    std::vector<std::string> w;
    ImportedSettings one = parse_text(merged.source_id, text, std::string(up.begin(), up.end()), w);
    for (auto& x : w) warnings.push_back(x);
    // Merge.
    merged.searches.insert(merged.searches.end(), one.searches.begin(), one.searches.end());
    merged.snippets.insert(merged.snippets.end(), one.snippets.begin(), one.snippets.end());
    merged.aliases.insert(merged.aliases.end(), one.aliases.begin(), one.aliases.end());
    for (auto& [k, v] : one.quicklinks)
      if (!merged.quicklinks.count(k)) merged.quicklinks[k] = v;
    if (!merged.hotkey.has && one.hotkey.has) merged.hotkey = one.hotkey;
    if (!merged.theme && one.theme) merged.theme = one.theme;
    if (!merged.default_search_template && one.default_search_template)
      merged.default_search_template = one.default_search_template;
    ++files;
  }
  if (files == 0)
    warnings.push_back("No parseable files found under " + dir_path + ".");
  else if (merged.searches.empty() && merged.snippets.empty() && merged.quicklinks.empty() &&
           !merged.hotkey.has)
    warnings.push_back("Scanned " + std::to_string(files) + " file(s) under " + dir_path +
                       " but found nothing importable.");
  return merged;
}

bool parse_file_auto(const std::string& path, ImportedSettings& out, std::string& detected_id,
                     std::string& error) {
  std::error_code ec;
  bool is_dir = fs::is_directory(fs::path(path), ec) && !ec;
  if (is_dir) {
    // Guess launcher from directory name, else auto.
    std::string guess = detect_id_for_path(path, "");
    if (guess == "auto") {
      // Look inside for characteristic files.
      for (auto it = fs::directory_iterator(fs::path(path), ec);
           it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) break;
        std::string n = lower(it->path().filename().string());
        if (n.find("alfred") != std::string::npos) {
          guess = "alfred";
          break;
        }
      }
      if (guess == "auto") guess = "alfred";  // bundles are the common directory case
    }
    std::vector<std::string> w;
    out = parse_directory(guess, path, w);
    out.warnings.insert(out.warnings.end(), w.begin(), w.end());
    detected_id = guess;
    if (out.searches.empty() && out.snippets.empty() && out.quicklinks.empty() && !out.hotkey.has &&
        !out.theme) {
      error = "no importable settings found under " + path;
      return false;
    }
    return true;
  }
  std::string text;
  if (!read_file_all(path, text)) {
    error = "unable to read " + path;
    return false;
  }
  if (text.empty()) {
    error = "file is empty: " + path;
    return false;
  }
  detected_id = detect_id_for_path(path, text);
  std::vector<std::string> w;
  if (detected_id == "auto") {
    out = parse_text("auto", text, path, w);
    out.source_id = "auto";
    out.source_name = "Autodetected";
  } else {
    out = parse_text(detected_id, text, path, w);
  }
  out.warnings.insert(out.warnings.end(), w.begin(), w.end());
  if (out.searches.empty() && out.snippets.empty() && out.quicklinks.empty() &&
      out.aliases.empty() && !out.hotkey.has && !out.theme && !out.default_search_template) {
    error = "no importable settings found in " + path + " (detected as " + detected_id + ")";
    return false;
  }
  return true;
}

ImportCounts apply_settings(Config& cfg, const ImportedSettings& in, const ImportOptions& opts,
                            std::vector<std::string>& warnings) {
  ImportCounts c;
  for (auto& w : in.warnings) warnings.push_back(w);
  auto sanitize_macro_key = [](std::string k) {
    k = lower(trim(k));
    std::string o;
    for (char ch : k) {
      if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
        o.push_back(ch);
    }
    if (o.empty()) o = slugify_keyword(k);
    return o;
  };
  if (opts.include_searches) {
    // Track keys added in this batch to avoid double-reporting mirrors.
    for (auto& w : in.searches) {
      if (w.url.empty()) {
        warnings.push_back("Skipped search '" + w.name + "': empty URL.");
        continue;
      }
      std::string key = w.keyword.empty() ? slugify_keyword(w.name) : sanitize_macro_key(w.keyword);
      if (key.empty()) {
        warnings.push_back("Skipped search '" + w.name + "': empty keyword.");
        continue;
      }
      std::string tpl = normalize_url_template(w.url);
      if (tpl.find("http") != 0) {
        warnings.push_back("Skipped search '" + key + "': not an http(s) URL (" + w.url + ").");
        continue;
      }
      auto it = cfg.macros.find(key);
      // Also check case-insensitive? Config stores lowercased keys already.
      if (it == cfg.macros.end()) {
        cfg.macros[key] = tpl;
        ++c.macros_added;
      } else if (it->second == tpl) {
        ++c.macros_skipped;  // identical, nothing to do
      } else if (opts.overwrite) {
        it->second = tpl;
        ++c.macros_overwritten;
      } else {
        ++c.macros_skipped;
        warnings.push_back("Macro '" + key + "' already exists; kept existing (use "
                           "--overwrite to replace).");
      }
      if (opts.include_quicklinks) {
        auto qit = cfg.quicklinks.find(key);
        if (qit == cfg.quicklinks.end()) {
          cfg.quicklinks[key] = tpl;
          ++c.quicklinks_added;
        } else if (qit->second == tpl) {
          ++c.quicklinks_skipped;
        } else if (opts.overwrite) {
          qit->second = tpl;
          ++c.quicklinks_overwritten;
        } else {
          ++c.quicklinks_skipped;
        }
      }
    }
  }
  if (opts.include_quicklinks) {
    for (auto& [k, tpl_raw] : in.quicklinks) {
      std::string key = sanitize_macro_key(k);
      if (key.empty()) continue;
      // Skip if this key was already handled as a mirrored search with same template.
      // (apply_settings counts mirrors above; here only count extras.)
      bool mirrored = false;
      if (opts.include_searches) {
        for (auto& w : in.searches) {
          std::string wk = w.keyword.empty() ? slugify_keyword(w.name) : sanitize_macro_key(w.keyword);
          if (wk == key && normalize_url_template(w.url) == tpl_raw) {
            mirrored = true;
            break;
          }
        }
      }
      if (mirrored) continue;
      std::string tpl = tpl_raw;
      // Quicklink templates may be commands/paths; accept as-is if non-empty.
      if (tpl.empty()) {
        warnings.push_back("Skipped quicklink '" + key + "': empty template.");
        continue;
      }
      auto it = cfg.quicklinks.find(key);
      if (it == cfg.quicklinks.end()) {
        cfg.quicklinks[key] = tpl;
        ++c.quicklinks_added;
      } else if (it->second == tpl) {
        ++c.quicklinks_skipped;
      } else if (opts.overwrite) {
        it->second = tpl;
        ++c.quicklinks_overwritten;
      } else {
        ++c.quicklinks_skipped;
        warnings.push_back("Quicklink '" + key + "' already exists; kept existing.");
      }
    }
  }
  if (opts.include_snippets) {
    // Build lowercase index of existing triggers for case-insensitive compare.
    std::map<std::string, std::string> existing;  // lower -> actual key
    for (auto& [k, v] : cfg.snippets.items) existing[lower(k)] = k;
    for (auto& sn : in.snippets) {
      if (sn.trigger.empty() || sn.body.empty()) {
        warnings.push_back("Skipped snippet with empty trigger/body.");
        continue;
      }
      std::string trig = trim(sn.trigger);
      if (trig.size() > 64) {
        warnings.push_back("Skipped snippet '" + trig + "': trigger too long.");
        continue;
      }
      auto it = existing.find(lower(trig));
      if (it == existing.end()) {
        cfg.snippets.items[trig] = sn.body;
        existing[lower(trig)] = trig;
        ++c.snippets_added;
      } else if (cfg.snippets.items[it->second] == sn.body) {
        ++c.snippets_skipped;
      } else if (opts.overwrite) {
        cfg.snippets.items[it->second] = sn.body;
        ++c.snippets_overwritten;
      } else {
        ++c.snippets_skipped;
        warnings.push_back("Snippet '" + trig + "' already exists; kept existing.");
      }
    }
  }
  if (opts.include_aliases) {
    for (auto& a : in.aliases) {
      std::string from = lower(trim(a.from));
      if (from.empty() || a.to.empty()) continue;
      auto it = cfg.aliases.find(from);
      if (it == cfg.aliases.end()) {
        cfg.aliases[from] = a.to;
        ++c.aliases_added;
      } else if (it->second == a.to) {
        ++c.aliases_skipped;
      } else if (opts.overwrite) {
        it->second = a.to;
        ++c.aliases_overwritten;
      } else {
        ++c.aliases_skipped;
      }
    }
  }
  if (opts.include_hotkey && in.hotkey.has) {
    if (in.hotkey.modifiers.empty() || in.hotkey.key.empty()) {
      warnings.push_back("Skipped hotkey: incomplete binding.");
    } else {
      cfg.hotkey.modifiers = in.hotkey.modifiers;
      cfg.hotkey.key = in.hotkey.key;
      c.hotkey_applied = true;
    }
  }
  if (opts.include_theme && in.theme) {
    std::string l = lower(trim(*in.theme));
    if (l == "dark" || l == "light") {
      cfg.ui.theme = l;
      c.theme_applied = true;
    } else {
      warnings.push_back("Skipped theme '" + *in.theme + "': expected dark/light.");
    }
  }
  if (opts.include_browser && in.default_search_template) {
    std::string tpl = normalize_url_template(*in.default_search_template);
    if (tpl.find("{query}") == std::string::npos) {
      warnings.push_back("Skipped default search template: missing {query} placeholder.");
    } else {
      cfg.browser.search_template = tpl;
      c.browser_applied = true;
    }
  }
  return c;
}

std::string serialize_config(const Config& cfg) {
  std::ostringstream os;
  os << "# Wilfred configuration (written by `wilfred import`; comments not preserved).\n";
  os << "# Edit freely; invalid values produce human-readable errors at startup.\n\n";
  os << "search:\n";
  os << "  include_system_files: " << (cfg.search.include_system_files ? "true" : "false") << "\n";
  os << "  include_hidden_files: " << (cfg.search.include_hidden_files ? "true" : "false") << "\n";
  os << "  show_system_in_results: " << (cfg.search.show_system_in_results ? "true" : "false")
     << "\n";
  os << "  max_results: " << cfg.search.max_results << "\n";
  os << "  debounce_ms: " << cfg.search.debounce_ms << "\n";
  os << "  web_search_fallback: " << (cfg.search.web_search_fallback ? "true" : "false") << "\n";
  os << "  treat_urls_as_open: " << (cfg.search.treat_urls_as_open ? "true" : "false") << "\n";
  os << "  min_query_length: " << cfg.search.min_query_length << "\n";
  os << "  fuzzy: " << (cfg.search.fuzzy ? "true" : "false") << "\n";
  os << "  acronyms: " << (cfg.search.acronyms ? "true" : "false") << "\n";
  os << "  context_aware: " << (cfg.search.context_aware ? "true" : "false") << "\n";
  os << "  clipboard: " << (cfg.search.clipboard ? "true" : "false") << "\n";
  os << "  minis: " << (cfg.search.minis ? "true" : "false") << "\n";
  os << "  macros: " << (cfg.search.macros ? "true" : "false") << "\n";
  os << "  snippets: " << (cfg.search.snippets ? "true" : "false") << "\n";
  os << "  plugins: " << (cfg.search.plugins ? "true" : "false") << "\n";
  os << "\nindex:\n";
  emit_str_list(os, "paths", cfg.index.paths, 2);
  emit_str_list(os, "exclude", cfg.index.exclude, 2);
  emit_str_list(os, "exclude_globs", cfg.index.exclude_globs, 2);
  emit_str_list(os, "system_directories", cfg.index.system_directories, 2);
  os << "  follow_symlinks: " << (cfg.index.follow_symlinks ? "true" : "false") << "\n";
  os << "  index_hidden: " << (cfg.index.index_hidden ? "true" : "false") << "\n";
  os << "  index_system: " << (cfg.index.index_system ? "true" : "false") << "\n";
  os << "  usn_scan: " << (cfg.index.usn_scan ? "true" : "false") << "\n";
  os << "  max_file_size_bytes: " << cfg.index.max_file_size_bytes << "\n";
  os << "  content_indexing: " << (cfg.index.content_indexing ? "true" : "false") << "\n";
  os << "  content_max_bytes: " << cfg.index.content_max_bytes << "\n";
  os << "  content_max_tokens: " << cfg.index.content_max_tokens << "\n";
  os << "  workers: " << cfg.index.workers << "\n";
  os << "  cpu_percent_limit: " << cfg.index.cpu_percent_limit << "\n";
  os << "  memory_limit_mb: " << cfg.index.memory_limit_mb << "\n";
  os << "  batch_size: " << cfg.index.batch_size << "\n";
  os << "  debounce_fs_ms: " << cfg.index.debounce_fs_ms << "\n";
  os << "  rescan_interval_seconds: " << cfg.index.rescan_interval_seconds << "\n";
  os << "  persist_every_records: " << cfg.index.persist_every_records << "\n";
  os << "  wal_compact_bytes: " << cfg.index.wal_compact_bytes << "\n";
  os << "  extensions:\n";
  emit_str_list(os, "include", cfg.index.ext_include, 4);
  emit_str_list(os, "exclude", cfg.index.ext_exclude, 4);
  os << "\nranking:\n";
  os << "  exact_name: " << cfg.ranking.exact_name << "\n";
  os << "  prefix_name: " << cfg.ranking.prefix_name << "\n";
  os << "  substring_name: " << cfg.ranking.substring_name << "\n";
  os << "  fuzzy_name: " << cfg.ranking.fuzzy_name << "\n";
  os << "  acronym: " << cfg.ranking.acronym << "\n";
  os << "  path_component: " << cfg.ranking.path_component << "\n";
  os << "  extension: " << cfg.ranking.extension << "\n";
  os << "  application: " << cfg.ranking.application << "\n";
  os << "  recency: " << cfg.ranking.recency << "\n";
  os << "  frequency: " << cfg.ranking.frequency << "\n";
  os << "  previous_selection: " << cfg.ranking.previous_selection << "\n";
  os << "  word_boundary: " << cfg.ranking.word_boundary << "\n";
  os << "  token_proximity: " << cfg.ranking.token_proximity << "\n";
  os << "  directory_bonus: " << cfg.ranking.directory_bonus << "\n";
  os << "  alias: " << cfg.ranking.alias << "\n";
  os << "  learned_choice: " << cfg.ranking.learned_choice << "\n";
  os << "  context_parent: " << cfg.ranking.context_parent << "\n";
  os << "  context_extension: " << cfg.ranking.context_extension << "\n";
  os << "  access_recency: " << cfg.ranking.access_recency << "\n";
  os << "  clipboard_overlap: " << cfg.ranking.clipboard_overlap << "\n";
  os << "  content_hit: " << cfg.ranking.content_hit << "\n";
  os << "  hour_affinity: " << cfg.ranking.hour_affinity << "\n";
  if (!cfg.aliases.empty()) {
    os << "\n";
    emit_map_unordered(os, "aliases", cfg.aliases, 0);
  }
  if (!cfg.macros.empty()) {
    os << "\n";
    emit_map_unordered(os, "macros", cfg.macros, 0);
  }
  if (!cfg.scopes.empty()) {
    os << "\nscopes:\n";
    std::map<std::string, std::vector<std::string>> sorted;
    for (auto& [k, v] : cfg.scopes) sorted[k] = v;
    for (auto& [k, v] : sorted) emit_str_list(os, k.c_str(), v, 2);
  }
  if (!cfg.custom_metadata.empty()) {
    os << "\n";
    std::map<std::string, std::string> cm(cfg.custom_metadata.begin(), cfg.custom_metadata.end());
    emit_map(os, "custom_metadata", cm, 0);
  }
  os << "\nhistory:\n";
  os << "  enabled: " << (cfg.history.enabled ? "true" : "false") << "\n";
  os << "  max_entries: " << cfg.history.max_entries << "\n";
  os << "  persist: " << (cfg.history.persist ? "true" : "false") << "\n";
  os << "\nclipboard:\n";
  os << "  manager: " << (cfg.clipboard.manager ? "true" : "false") << "\n";
  os << "  max_entries: " << cfg.clipboard.max_entries << "\n";
  os << "  persist: " << (cfg.clipboard.persist ? "true" : "false") << "\n";
  os << "\nhotkey:\n";
  os << "  enabled: " << (cfg.hotkey.enabled ? "true" : "false") << "\n";
  emit_str_list(os, "modifiers", cfg.hotkey.modifiers, 2);
  os << "  key: " << yq(cfg.hotkey.key) << "\n";
  os << "  use_command_on_macos: " << (cfg.hotkey.use_command_on_macos ? "true" : "false") << "\n";
  if (!cfg.hotkeys.empty()) {
    os << "\nhotkeys:\n";
    for (auto& b : cfg.hotkeys) {
      os << "  " << ykey(b.name) << ":\n";
      emit_str_list(os, "modifiers", b.modifiers, 4);
      os << "    key: " << yq(b.key) << "\n";
      os << "    run: " << yq(b.run) << "\n";
    }
  }
  os << "\nbrowser:\n";
  os << "  provider: " << yq(cfg.browser.provider) << "\n";
  os << "  search_template: " << yq(cfg.browser.search_template) << "\n";
  os << "  library: " << (cfg.browser.library ? "true" : "false") << "\n";
  os << "\nlogging:\n";
  os << "  level: " << yq(cfg.logging.level) << "\n";
  os << "  file: " << yq(cfg.logging.file) << "\n";
  os << "  max_file_bytes: " << cfg.logging.max_file_bytes << "\n";
  os << "\nui:\n";
  os << "  theme: " << yq(cfg.ui.theme) << "\n";
  os << "  max_visible: " << cfg.ui.max_visible << "\n";
  os << "  width: " << cfg.ui.width << "\n";
  os << "\nplugins:\n";
  os << "  enabled: " << (cfg.plugins.enabled ? "true" : "false") << "\n";
  emit_str_list(os, "directories", cfg.plugins.directories, 2);
  os << "  timeout_ms: " << cfg.plugins.timeout_ms << "\n";
  os << "\nproviders:\n";
  os << "  semantic: " << (cfg.providers.semantic ? "true" : "false") << "\n";
  os << "  semantic_min_score: " << cfg.providers.semantic_min_score << "\n";
  os << "  semantic_backend: " << yq(cfg.providers.semantic_backend) << "\n";
  os << "  semantic_max_results: " << cfg.providers.semantic_max_results << "\n";
  os << "\nembedding:\n";
  os << "  enabled: " << (cfg.embedding.enabled ? "true" : "false") << "\n";
  os << "  backend: " << yq(cfg.embedding.backend) << "\n";
  os << "  model: " << yq(cfg.embedding.model) << "\n";
  os << "  endpoint: " << yq(cfg.embedding.endpoint) << "\n";
  os << "  dim: " << cfg.embedding.dim << "\n";
  os << "  min_score: " << cfg.embedding.min_score << "\n";
  os << "  max_results: " << cfg.embedding.max_results << "\n";
  os << "\nai:\n";
  os << "  enabled: " << (cfg.ai.enabled ? "true" : "false") << "\n";
  os << "  provider: " << yq(cfg.ai.provider) << "\n";
  os << "  model: " << yq(cfg.ai.model) << "\n";
  os << "  api_key: " << yq(cfg.ai.api_key) << "\n";
  os << "  endpoint: " << yq(cfg.ai.endpoint) << "\n";
  os << "  max_tokens: " << cfg.ai.max_tokens << "\n";
  os << "  temperature: " << cfg.ai.temperature << "\n";
  os << "  timeout_ms: " << cfg.ai.timeout_ms << "\n";
  os << "\nsources:\n";
  os << "  calendar: " << (cfg.sources.calendar ? "true" : "false") << "\n";
  os << "  contacts: " << (cfg.sources.contacts ? "true" : "false") << "\n";
  os << "  notes: " << (cfg.sources.notes ? "true" : "false") << "\n";
  emit_str_list(os, "calendar_paths", cfg.sources.calendar_paths, 2);
  emit_str_list(os, "contacts_paths", cfg.sources.contacts_paths, 2);
  emit_str_list(os, "notes_paths", cfg.sources.notes_paths, 2);
  os << "  ocr: " << (cfg.sources.ocr ? "true" : "false") << "\n";
  os << "  ocr_languages: " << yq(cfg.sources.ocr_languages) << "\n";
  os << "  max_results: " << cfg.sources.max_results << "\n";
  os << "\ntranscription:\n";
  os << "  enabled: " << (cfg.transcription.enabled ? "true" : "false") << "\n";
  os << "  binary: " << yq(cfg.transcription.binary) << "\n";
  os << "  model: " << yq(cfg.transcription.model) << "\n";
  os << "  language: " << yq(cfg.transcription.language) << "\n";
  os << "  save_txt: " << (cfg.transcription.save_txt ? "true" : "false") << "\n";
  os << "  mic: " << yq(cfg.transcription.mic) << "\n";
  os << "\napi:\n";
  os << "  enabled: " << (cfg.api.enabled ? "true" : "false") << "\n";
  os << "  bind: " << yq(cfg.api.bind) << "\n";
  os << "  port: " << cfg.api.port << "\n";
  os << "  token: " << yq(cfg.api.token) << "\n";
  os << "\nsync:\n";
  os << "  enabled: " << (cfg.sync.enabled ? "true" : "false") << "\n";
  os << "  url: " << yq(cfg.sync.url) << "\n";
  os << "  token: " << yq(cfg.sync.token) << "\n";
  os << "  interval_seconds: " << cfg.sync.interval_seconds << "\n";
  os << "  include_index: " << (cfg.sync.include_index ? "true" : "false") << "\n";
  os << "\nsnippets:\n";
  os << "  expansion: " << (cfg.snippets.expansion ? "true" : "false") << "\n";
  os << "  prefix: " << yq(cfg.snippets.prefix) << "\n";
  os << "  auto_paste: " << (cfg.snippets.auto_paste ? "true" : "false") << "\n";
  os << "  global_expansion: " << (cfg.snippets.global_expansion ? "true" : "false") << "\n";
  if (!cfg.snippets.items.empty()) {
    os << "  items:\n";
    std::map<std::string, std::string> sorted(cfg.snippets.items.begin(),
                                              cfg.snippets.items.end());
    for (auto& [k, v] : sorted) os << "    " << ykey(k) << ": " << yq(v) << "\n";
  }
  if (!cfg.workflows.empty()) {
    os << "\nworkflows:\n";
    std::map<std::string, std::vector<std::string>> sorted(cfg.workflows.begin(),
                                                           cfg.workflows.end());
    for (auto& [k, v] : sorted) {
      os << "  " << ykey(k) << ":\n";
      for (auto& st : v) os << "    - " << yq(st) << "\n";
    }
  }
  if (!cfg.quicklinks.empty()) {
    os << "\nquicklinks:\n";
    std::map<std::string, std::string> sorted(cfg.quicklinks.begin(), cfg.quicklinks.end());
    for (auto& [k, v] : sorted) os << "  " << ykey(k) << ": " << yq(v) << "\n";
  }
  if (!cfg.app_actions.empty()) {
    os << "\napp_actions:\n";
    std::map<std::string, std::vector<std::string>> sorted(cfg.app_actions.begin(),
                                                           cfg.app_actions.end());
    for (auto& [k, v] : sorted) {
      os << "  " << ykey(k) << ":\n";
      for (auto& st : v) os << "    - " << yq(st) << "\n";
    }
  }
  // Trailing newline keeps the file POSIX-clean; the last section above is
  // always non-bare (ends with a value), so the minimal YAML parser accepts it.
  os << "\n";
  return os.str();
}

bool save_config_file(const std::string& path, const Config& cfg, std::string& error) {
  // Validate by round-tripping through the parser before touching disk.
  std::string text = serialize_config(cfg);
  Config check;
  ConfigError cerr;
  if (!load_config_text(text, check, cerr)) {
    error = "generated config failed validation: " + cerr.message;
    return false;
  }
  create_directories(path_parent(path));
  if (!write_file_atomic(path, text.data(), text.size())) {
    error = "unable to write " + path;
    return false;
  }
  return true;
}

}  // namespace wilfred::import
