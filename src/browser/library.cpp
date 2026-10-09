#include "wilfred/browser/library.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/platform/platform.hpp"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {
namespace fs = std::filesystem;

namespace {

std::string shell_dquote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') o.push_back('\\');
    o.push_back(c);
  }
  o.push_back('"');
  return o;
}

#ifndef _WIN32
bool have_tool(const char* name) {
#if defined(WILFRED_IOS)
  (void)name;
  return false;  // no process spawning in the iOS sandbox
#else
  std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
#endif
}
#endif

// Extract the JSON object starting at the given '{' (brace matching,
// string-aware). Returns empty when unbalanced.
std::string brace_object(const std::string& s, std::size_t open) {
  if (open >= s.size() || s[open] != '{') return {};
  int depth = 0;
  bool in_str = false, esc = false;
  for (std::size_t i = open; i < s.size(); ++i) {
    char c = s[i];
    if (in_str) {
      if (esc)
        esc = false;
      else if (c == '\\')
        esc = true;
      else if (c == '"')
        in_str = false;
    } else if (c == '"') {
      in_str = true;
    } else if (c == '{') {
      ++depth;
    } else if (c == '}') {
      if (--depth == 0) return s.substr(open, i - open + 1);
    }
  }
  return {};
}

// Walk Chromium Bookmarks JSON: every {"type":"url",...} object contributes
// its name/url, wherever nested. Bounded and dependency-free.
void collect_chromium_urls(const std::string& text, std::vector<LibraryItem>& out,
                           std::size_t cap) {
  std::size_t pos = 0;
  while (out.size() < cap) {
    auto t = text.find("\"type\"", pos);
    if (t == std::string::npos) break;
    auto c = text.find(':', t + 6);
    if (c == std::string::npos) break;
    std::size_t v = c + 1;
    while (v < text.size() && (text[v] == ' ' || text[v] == '\t'))
      ++v;
    bool is_url = text.compare(v, 5, "\"url\"") == 0;
    // Enclosing object: scan backwards for the nearest unmatched '{'.
    // (Start before the key itself so its quotes don't skew parity.)
    std::size_t open = std::string::npos;
    if (t > 0) {
      int depth = 0;
      bool in_str = false, esc = false;
      std::size_t i = t;
      while (i > 0) {
        --i;
        char ch = text[i];
        if (in_str) {
          if (esc)
            esc = false;
          else if (ch == '\\')
            esc = true;
          else if (ch == '"')
            in_str = false;
        } else if (ch == '"') {
          in_str = true;
        } else if (ch == '}') {
          ++depth;
        } else if (ch == '{') {
          if (depth == 0) {
            open = i;
            break;
          }
          --depth;
        }
      }
    }
    pos = t + 6;
    if (!is_url || open == std::string::npos) continue;
    auto obj = brace_object(text, open);
    if (obj.empty()) continue;
    auto url = json_get_string(obj, "url");
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) continue;
    LibraryItem it;
    it.url = url;
    it.title = json_get_string(obj, "name");
    if (it.title.empty()) it.title = url;
    it.source = "bookmark";
    out.push_back(std::move(it));
  }
}

std::vector<std::string> chromium_profile_dirs() {
  std::vector<std::string> bases;
#ifdef _WIN32
  auto local = home_directory();
  // %LOCALAPPDATA% roots.
  wchar_t* env = nullptr;
  std::size_t n = 0;
  if (_wdupenv_s(&env, &n, L"LOCALAPPDATA") == 0 && env) {
    std::string lad = wide_to_utf8(env);
    free(env);
    bases.push_back(path_join(lad, "Google/Chrome/User Data"));
    bases.push_back(path_join(lad, "Microsoft/Edge/User Data"));
    bases.push_back(path_join(lad, "BraveSoftware/Brave-Browser/User Data"));
    bases.push_back(path_join(lad, "Chromium/User Data"));
    bases.push_back(path_join(lad, "Vivaldi/User Data"));
  }
  if (_wdupenv_s(&env, &n, L"APPDATA") == 0 && env) {
    std::string ad = wide_to_utf8(env);
    free(env);
    bases.push_back(path_join(ad, "Opera Software/Opera Stable"));
  }
  (void)local;
#elif defined(__APPLE__)
  auto sup = path_join(home_directory(), "Library/Application Support");
  bases.push_back(path_join(sup, "Google/Chrome"));
  bases.push_back(path_join(sup, "Microsoft Edge"));
  bases.push_back(path_join(sup, "BraveSoftware/Brave-Browser"));
  bases.push_back(path_join(sup, "Chromium"));
  bases.push_back(path_join(sup, "Vivaldi"));
  bases.push_back(path_join(sup, "Opera"));
#else
  auto cfg = path_join(home_directory(), ".config");
  bases.push_back(path_join(cfg, "google-chrome"));
  bases.push_back(path_join(cfg, "microsoft-edge"));
  bases.push_back(path_join(cfg, "BraveSoftware/Brave-Browser"));
  bases.push_back(path_join(cfg, "chromium"));
  bases.push_back(path_join(cfg, "vivaldi"));
  bases.push_back(path_join(cfg, "opera"));
#endif
  return bases;
}

void add_bookmarks_file(const std::string& file, std::vector<LibraryItem>& out, std::size_t cap) {
  parse_chromium_bookmarks_file(file, out, cap);
}

void collect_chromium(std::vector<LibraryItem>& out, std::size_t cap) {
  for (auto& base : chromium_profile_dirs()) {
    if (out.size() >= cap) break;
    std::error_code ec;
    if (!fs::is_directory(fs::u8path(base), ec)) continue;
    // Top-level Bookmarks (some channels) plus per-profile subdirs.
    add_bookmarks_file(path_join(base, "Bookmarks"), out, cap);
    if (out.size() >= cap) break;
    for (auto it = fs::directory_iterator(fs::u8path(base), ec);
         it != fs::directory_iterator() && out.size() < cap; it.increment(ec)) {
      if (ec) break;
      std::error_code ec2;
      if (!it->is_directory(ec2)) continue;
      auto fn_u8 = it->path().filename().u8string();
      std::string fn(fn_u8.begin(), fn_u8.end());
      if (fn != "Default" && fn.rfind("Profile ", 0) != 0 && fn.rfind("Guest Profile", 0) != 0)
        continue;
      auto fp_u8 = it->path().u8string();
      add_bookmarks_file(path_join(std::string(fp_u8.begin(), fp_u8.end()), "Bookmarks"), out, cap);
    }
  }
}

bool copy_file(const std::string& from, const std::string& to) {
  std::error_code ec;
  fs::copy_file(fs::u8path(from), fs::u8path(to), fs::copy_options::overwrite_existing, ec);
  return !ec;
}

// Run a sqlite3 CLI query (databases are often locked, so copy first).
// Columns separated by \t, rows by \n. Empty when sqlite3 is unavailable.
std::string sqlite_query(const std::string& db, const std::string& sql) {
#ifndef _WIN32
  if (!have_tool("sqlite3")) return {};
#else
  {
    // Look for sqlite3.exe on PATH.
    WCHAR full[MAX_PATH]{};
    if (!SearchPathW(nullptr, L"sqlite3.exe", nullptr, MAX_PATH, full, nullptr)) return {};
  }
#endif
  create_directories(data_directory());
  static std::atomic<int> seq{0};
  auto dst = path_join(data_directory(), "sqlite_tmp_" + std::to_string(++seq) + ".db");
  if (!copy_file(db, dst)) return {};
  std::string cmd = "sqlite3 -separator \t " + shell_dquote(dst) + " " + shell_dquote(sql);
#ifdef _WIN32
  FILE* f = _popen(cmd.c_str(), "r");
#else
  FILE* f = popen(cmd.c_str(), "r");
#endif
  std::string out;
  if (f) {
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), f)) {
      out += buf;
      if (out.size() > 256 * 1024) break;
    }
#ifdef _WIN32
    _pclose(f);
#else
    pclose(f);
#endif
  }
  std::error_code ec;
  fs::remove(fs::u8path(dst), ec);
  return out;
}

void add_rows(std::vector<LibraryItem>& out, const std::string& rows, const std::string& source,
              std::size_t cap) {
  std::size_t i = 0;
  while (out.size() < cap && i < rows.size()) {
    auto nl = rows.find('\n', i);
    std::string line = rows.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
    i = nl == std::string::npos ? rows.size() : nl + 1;
    auto tab = line.find('\t');
    if (tab == std::string::npos) continue;
    std::string title = line.substr(0, tab);
    std::string url = line.substr(tab + 1);
    while (!url.empty() && (url.back() == '\n' || url.back() == '\r'))
      url.pop_back();
    if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) continue;
    LibraryItem it;
    it.title = title.empty() ? url : title;
    it.url = url;
    it.source = source;
    out.push_back(std::move(it));
  }
}

void collect_chromium_history(std::vector<LibraryItem>& out, std::size_t cap) {
  for (auto& base : chromium_profile_dirs()) {
    if (out.size() >= cap) break;
    std::error_code ec;
    if (!fs::is_directory(fs::u8path(base), ec)) continue;
    std::vector<std::string> dbs;
    dbs.push_back(path_join(base, "History"));
    dbs.push_back(path_join(base, "Default/History"));
    for (auto& db : dbs) {
      if (out.size() >= cap || !file_exists(db)) continue;
      auto rows =
          sqlite_query(db, "SELECT title,url FROM urls ORDER BY last_visit_time DESC LIMIT 300;");
      add_rows(out, rows, "history", cap);
    }
  }
}

void collect_firefox(std::vector<LibraryItem>& out, bool history, std::size_t cap) {
  std::string ini;
#ifdef _WIN32
  wchar_t* env = nullptr;
  std::size_t n = 0;
  std::string ad;
  if (_wdupenv_s(&env, &n, L"APPDATA") == 0 && env) {
    ad = wide_to_utf8(env);
    free(env);
  } else {
    return;
  }
  ini = path_join(path_join(ad, "Mozilla/Firefox"), "profiles.ini");
#elif defined(__APPLE__)
  ini =
      path_join(path_join(home_directory(), "Library/Application Support/Firefox"), "profiles.ini");
#else
  ini = path_join(path_join(home_directory(), ".mozilla/firefox"), "profiles.ini");
#endif
  std::string text;
  if (!read_file_all(ini, text)) return;
  std::string dir = path_parent(ini);
  std::size_t pos = 0;
  while (out.size() < cap) {
    auto p = text.find("Path=", pos);
    if (p == std::string::npos) break;
    pos = p + 5;
    auto e = text.find('\n', pos);
    std::string rel = text.substr(pos, e == std::string::npos ? std::string::npos : e - pos);
    while (!rel.empty() && (rel.back() == '\r' || rel.back() == ' '))
      rel.pop_back();
    if (rel.empty()) continue;
    auto prof = path_is_absolute(rel) ? rel : path_join(dir, rel);
    auto db = path_join(prof, "places.sqlite");
    if (!file_exists(db)) continue;
    auto rows =
        sqlite_query(db,
                     "SELECT b.title,p.url FROM moz_bookmarks b JOIN moz_places p ON b.fk=p.id "
                     "WHERE b.type=1 AND p.url LIKE 'http%' LIMIT 500;");
    add_rows(out, rows, "bookmark", cap);
    if (history && out.size() < cap) {
      auto hrows = sqlite_query(db,
                                "SELECT title,url FROM moz_places WHERE url LIKE 'http%' "
                                "ORDER BY last_visit_date DESC LIMIT 300;");
      add_rows(out, hrows, "history", cap);
    }
  }
}

void collect_tabs_macos(std::vector<LibraryItem>& out, std::size_t cap) {
#ifdef __APPLE__
  auto probe = [](const char* proc) {
    std::string cmd = std::string(
                          "osascript -e 'tell application \"System Events\" to (exists "
                          "process \"") +
                      proc + "\")' 2>/dev/null";
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return false;
    char buf[32]{};
    bool yes = fgets(buf, sizeof(buf), f) && std::string(buf).find("true") != std::string::npos;
    pclose(f);
    return yes;
  };
  auto grab = [&](const char* app, const char* script) {
    if (out.size() >= cap || !probe(app)) return;
    std::string cmd = std::string("osascript -e ") + shell_dquote(script) + " 2>/dev/null";
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return;
    char buf[4096];
    std::string acc;
    while (fgets(buf, sizeof(buf), f)) {
      acc += buf;
      if (acc.size() > 128 * 1024) break;
    }
    pclose(f);
    std::size_t i = 0;
    while (out.size() < cap && i < acc.size()) {
      auto nl = acc.find('\n', i);
      std::string line = acc.substr(i, nl == std::string::npos ? std::string::npos : nl - i);
      i = nl == std::string::npos ? acc.size() : nl + 1;
      auto tab = line.find('\t');
      if (tab == std::string::npos) continue;
      std::string title = line.substr(0, tab);
      std::string url = line.substr(tab + 1);
      if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) continue;
      LibraryItem it;
      it.title = title.empty() ? url : title;
      it.url = url;
      it.source = "tab";
      out.push_back(std::move(it));
    }
  };
  grab("Google Chrome",
       "tell application \"Google Chrome\" to repeat with w in windows\n"
       "repeat with t in tabs of w\n"
       "log (title of t & \"\\t\" & URL of t)\n"
       "end repeat\nend repeat");
  grab("Safari",
       "tell application \"Safari\" to repeat with w in windows\n"
       "repeat with t in tabs of w\n"
       "log (name of t & \"\\t\" & URL of t)\n"
       "end repeat\nend repeat");
#else
  (void)out;
  (void)cap;
#endif
}

struct LibraryCache {
  std::mutex mu;
  std::vector<LibraryItem> items;
  std::int64_t at{0};
  bool hist{false};
  bool tabs{false};
};

}  // namespace

bool parse_chromium_bookmarks_file(const std::string& file, std::vector<LibraryItem>& out,
                                   std::size_t cap) {
  std::string text;
  if (!read_file_all(file, text) || text.size() > 8 * 1024 * 1024) return false;
  auto n0 = out.size();
  collect_chromium_urls(text, out, cap);
  return out.size() > n0;
}

std::vector<LibraryItem> browser_library(bool include_history, bool include_tabs) {
  static LibraryCache cache;
  std::int64_t now = 0;
  {
    // Cheap clock: reuse file-loop guard via static atomic? Use time().
    now = static_cast<std::int64_t>(std::time(nullptr));
    std::lock_guard<std::mutex> lock(cache.mu);
    if (!cache.items.empty() && now - cache.at < 60 && cache.hist == include_history &&
        cache.tabs == include_tabs)
      return cache.items;
  }
  std::vector<LibraryItem> items;
  try {
    collect_chromium(items, 2000);
    collect_firefox(items, include_history, 2000);
    if (include_history) collect_chromium_history(items, 2000);
    if (include_tabs) collect_tabs_macos(items, 2000);
  } catch (...) {
  }
  std::lock_guard<std::mutex> lock(cache.mu);
  cache.items = items;
  cache.at = now;
  cache.hist = include_history;
  cache.tabs = include_tabs;
  return items;
}

std::vector<SearchResult> BrowserLibraryProvider::query(const std::string& text, const Config& cfg,
                                                        std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.browser.library || limit == 0) return out;
  auto q = fold_search(normalize_query(text));
  if (q.size() < 2) return out;
  for (auto& it : browser_library(true, true)) {
    if (out.size() >= limit) break;
    auto title = fold_search(it.title);
    auto url = to_lower_utf8(it.url);
    if (title.find(q) == std::string::npos && url.find(q) == std::string::npos) continue;
    SearchResult r;
    r.title = it.title;
    r.subtitle = it.url;
    r.path = it.url;
    r.payload = it.url;
    r.action = ResultAction::WebSearch;
    r.score = 2500;
    r.kind = FileKind::Unknown;
    r.kind_label = it.source;
    r.category = "browser";
    out.push_back(std::move(r));
  }
  return out;
}

}  // namespace wilfred
