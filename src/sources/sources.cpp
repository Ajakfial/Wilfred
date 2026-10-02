#include "wilfred/sources/sources.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/fuzzy.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <mutex>

namespace wilfred {
namespace fs = std::filesystem;

std::vector<std::string> default_calendar_roots() {
  std::vector<std::string> out;
  auto home = home_directory();
  if (home.empty()) return out;
#ifdef _WIN32
  out.push_back(path_join(home, "Documents"));
#else
#ifdef __APPLE__
  out.push_back(path_join(path_join(home, "Library"), "Calendars"));
#else
  out.push_back(path_join(path_join(home, ".thunderbird"), ""));
  out.push_back(path_join(path_join(path_join(home, ".local"), "share"), "evolution"));
  out.push_back(path_join(path_join(home, ".calendar"), ""));
#endif
  out.push_back(path_join(home, "Documents"));
#endif
  return out;
}

std::vector<std::string> default_contacts_roots() {
  std::vector<std::string> out;
  auto home = home_directory();
  if (home.empty()) return out;
#ifdef _WIN32
  out.push_back(path_join(home, "Contacts"));
  out.push_back(path_join(home, "Documents"));
#else
#ifdef __APPLE__
  out.push_back(path_join(path_join(home, "Library"), "Application Support/AddressBook"));
#else
  out.push_back(path_join(path_join(path_join(home, ".local"), "share"), "contacts"));
  out.push_back(path_join(path_join(home, ".thunderbird"), ""));
#endif
  out.push_back(path_join(home, "Documents"));
#endif
  return out;
}

std::vector<std::string> default_notes_roots() {
  std::vector<std::string> out;
  // Quick notes live here as real Markdown files, so they are searchable
  // both as quick-note cards and through the notes provider.
  out.push_back(path_join(data_directory(), "notes"));
  auto home = home_directory();
  if (home.empty()) return out;
  out.push_back(path_join(home, "Notes"));
  out.push_back(path_join(home, "Documents/Notes"));
#ifdef __APPLE__
  out.push_back(path_join(path_join(home, "Library"), "Group Containers/group.com.apple.notes"));
#endif
  return out;
}

namespace {

struct CalEvent {
  std::string summary;
  std::string start;
  std::string desc;
  std::string path;
};

struct Contact {
  std::string name;
  std::string email;
  std::string phone;
  std::string path;
};

struct Note {
  std::string title;
  std::string snippet;
  std::string path;
};

std::string unfold_ics(const std::string& text) {
  // ICS folds long lines with CRLF + space.
  std::string o;
  o.reserve(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    if ((text[i] == '\n' || text[i] == '\r')) {
      std::size_t j = i + 1;
      while (j < text.size() && (text[j] == '\r' || text[j] == '\n')) ++j;
      if (j < text.size() && (text[j] == ' ' || text[j] == '\t')) {
        i = j;  // continuation: skip the break entirely
        continue;
      }
      o.push_back('\n');
      i = j - 1;
    } else {
      o.push_back(text[i]);
    }
  }
  return o;
}

std::string ics_prop(const std::string& block, const char* name) {
  std::string key = std::string(name) + ":";
  std::string key2 = std::string(name) + ";";
  std::size_t pos = 0;
  while (pos < block.size()) {
    auto nl = block.find('\n', pos);
    std::string line = block.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
    if (line.rfind(key, 0) == 0) return line.substr(key.size());
    if (line.rfind(key2, 0) == 0) {
      auto c = line.find(':');
      if (c != std::string::npos) return line.substr(c + 1);
    }
    if (nl == std::string::npos) break;
    pos = nl + 1;
  }
  return {};
}

bool score_hit(const std::string& needle_folded, const std::string& hay, int& score_out) {
  auto h = fold_search(hay);
  if (needle_folded.empty()) return false;
  if (h.find(needle_folded) != std::string::npos) {
    score_out = 6400;
    return true;
  }
  auto fm = score_fuzzy(needle_folded, h, hay);
  if (fm.matched) {
    score_out = 4200 + fm.score;
    return true;
  }
  return false;
}

std::vector<std::string> roots_or(const std::vector<std::string>& cfg_paths,
                                   const std::vector<std::string>& defs,
                                   const char* ext_filter) {
  std::vector<std::string> roots = cfg_paths.empty() ? defs : cfg_paths;
  (void)ext_filter;
  std::vector<std::string> live;
  for (auto& r : roots) {
    if (r.empty()) continue;
    std::error_code ec;
    if (fs::exists(fs::u8path(r), ec)) live.push_back(r);
  }
  return live;
}

void walk_files(const std::vector<std::string>& roots, const char* ext, int depth_max,
                std::vector<std::string>& out, int cap = 600) {
  for (auto& r : roots) {
    if ((int)out.size() >= cap) break;
    std::error_code ec;
    std::vector<std::pair<std::string, int>> stack{{r, 0}};
    while (!stack.empty() && (int)out.size() < cap) {
      auto [cur, d] = stack.back();
      stack.pop_back();
      std::error_code ec2;
      auto st = fs::status(fs::u8path(cur), ec2);
      if (ec2) continue;
      if (fs::is_regular_file(st)) {
        if (ext && to_lower_utf8(path_extension(cur)) == ext) out.push_back(cur);
        continue;
      }
      if (!fs::is_directory(st) || d > depth_max) continue;
      for (auto it = fs::directory_iterator(fs::u8path(cur), ec2);
           it != fs::directory_iterator(); it.increment(ec2)) {
        if (ec2) break;
        if ((int)out.size() + (int)stack.size() > cap + 200) break;
        stack.emplace_back(it->path().string(), d + 1);
      }
    }
  }
}

}  // namespace

std::vector<SearchResult> CalendarProvider::query(const std::string& text, const Config& cfg,
                                                 std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.sources.calendar || limit == 0) return out;
  auto needle = fold_search(normalize_query(text));
  if (needle.size() < 2) return out;
  static std::mutex mu;
  static std::vector<CalEvent> cache;
  static std::int64_t cache_at = 0;
  auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::steady_clock::now().time_since_epoch())
                 .count();
  std::lock_guard<std::mutex> lock(mu);
  if (cache.empty() || now - cache_at > 60) {
    cache.clear();
    cache_at = now;
    auto roots = roots_or(cfg.sources.calendar_paths, default_calendar_roots(), ".ics");
    std::vector<std::string> files;
    walk_files(roots, ".ics", 4, files);
    for (auto& f : files) {
      std::string raw;
      if (!read_file_all(f, raw) || raw.size() > 4 * 1024 * 1024) continue;
      auto unfolded = unfold_ics(raw);
      std::size_t pos = 0;
      while (pos < unfolded.size() && cache.size() < 800) {
        auto b = unfolded.find("BEGIN:VEVENT", pos);
        if (b == std::string::npos) break;
        auto e = unfolded.find("END:VEVENT", b);
        if (e == std::string::npos) break;
        auto block = unfolded.substr(b, e - b);
        CalEvent ev;
        ev.summary = ics_prop(block, "SUMMARY");
        ev.start = ics_prop(block, "DTSTART");
        ev.desc = ics_prop(block, "DESCRIPTION");
        ev.path = f;
        if (!ev.summary.empty()) cache.push_back(std::move(ev));
        pos = e + 10;
      }
    }
  }
  int budget = (int)std::min<std::size_t>(limit, (std::size_t)cfg.sources.max_results);
  struct Hit {
    int score;
    std::size_t idx;
  };
  std::vector<Hit> hits;
  for (std::size_t i = 0; i < cache.size(); ++i) {
    int s = 0;
    if (score_hit(needle, cache[i].summary + " " + cache[i].desc, s))
      hits.push_back({s, i});
  }
  std::sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.score > b.score; });
  for (auto& h : hits) {
    if ((int)out.size() >= budget) break;
    auto& ev = cache[h.idx];
    SearchResult r;
    r.title = ev.summary;
    r.subtitle = ev.start.empty() ? "Calendar" : ev.start + " · Calendar";
    r.path = ev.path;
    r.payload = ev.summary + (ev.start.empty() ? "" : " (" + ev.start + ")");
    r.action = ResultAction::Open;
    r.score = h.score;
    r.kind_label = "event";
    r.category = "calendar";
    out.push_back(std::move(r));
  }
  return out;
}

std::vector<SearchResult> ContactsProvider::query(const std::string& text, const Config& cfg,
                                                 std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.sources.contacts || limit == 0) return out;
  auto needle = fold_search(normalize_query(text));
  if (needle.size() < 2) return out;
  static std::mutex mu;
  static std::vector<Contact> cache;
  static std::int64_t cache_at = 0;
  auto now = std::chrono::duration_cast<std::chrono::seconds>(
                 std::chrono::steady_clock::now().time_since_epoch())
                 .count();
  std::lock_guard<std::mutex> lock(mu);
  if (cache.empty() || now - cache_at > 60) {
    cache.clear();
    cache_at = now;
    auto roots = roots_or(cfg.sources.contacts_paths, default_contacts_roots(), ".vcf");
    std::vector<std::string> files;
    walk_files(roots, ".vcf", 4, files);
    for (auto& f : files) {
      std::string raw;
      if (!read_file_all(f, raw) || raw.size() > 4 * 1024 * 1024) continue;
      std::size_t pos = 0;
      while (pos < raw.size() && cache.size() < 1200) {
        auto b = raw.find("BEGIN:VCARD", pos);
        if (b == std::string::npos) break;
        auto e = raw.find("END:VCARD", b);
        if (e == std::string::npos) break;
        auto block = raw.substr(b, e - b);
        Contact c;
        c.path = f;
        // FN preferred, else N.
        {
          auto p = block.find("FN:");
          if (p != std::string::npos) {
            auto nl = block.find('\n', p);
            c.name = block.substr(p + 3, nl == std::string::npos ? std::string::npos : nl - p - 3);
            while (!c.name.empty() && (c.name.back() == '\r' || c.name.back() == '\n'))
              c.name.pop_back();
          }
        }
        auto grab_first = [&](const char* key) -> std::string {
          std::string k(key);
          auto p = block.find(k);
          if (p == std::string::npos) return {};
          auto colon = block.find(':', p);
          if (colon == std::string::npos) return {};
          auto nl = block.find('\n', colon);
          auto v = block.substr(colon + 1, nl == std::string::npos ? std::string::npos : nl - colon - 1);
          while (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();
          return v;
        };
        if (c.name.empty()) {
          auto n = grab_first("N:");
          // N:Last;First;;; → "First Last"
          auto sc = n.find(';');
          if (sc != std::string::npos) {
            auto last = n.substr(0, sc);
            auto rest = n.substr(sc + 1);
            auto sc2 = rest.find(';');
            auto first = sc2 == std::string::npos ? rest : rest.substr(0, sc2);
            c.name = first + (first.empty() || last.empty() ? "" : " ") + last;
          } else {
            c.name = n;
          }
        }
        c.email = grab_first("EMAIL");
        c.phone = grab_first("TEL");
        if (!c.name.empty() || !c.email.empty()) cache.push_back(std::move(c));
        pos = e + 9;
      }
    }
  }
  int budget = (int)std::min<std::size_t>(limit, (std::size_t)cfg.sources.max_results);
  struct Hit {
    int score;
    std::size_t idx;
  };
  std::vector<Hit> hits;
  for (std::size_t i = 0; i < cache.size(); ++i) {
    int s = 0;
    if (score_hit(needle, cache[i].name + " " + cache[i].email + " " + cache[i].phone, s))
      hits.push_back({s, i});
  }
  std::sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.score > b.score; });
  for (auto& h : hits) {
    if ((int)out.size() >= budget) break;
    auto& c = cache[h.idx];
    SearchResult r;
    r.title = c.name.empty() ? c.email : c.name;
    std::string sub = c.email;
    if (!c.phone.empty()) sub += (sub.empty() ? "" : " · ") + c.phone;
    if (sub.empty()) sub = "Contact";
    else sub += " · Contact";
    r.subtitle = sub;
    r.path = c.path;
    r.payload = c.email.empty() ? c.name : c.email;
    r.action = ResultAction::Copy;
    r.score = h.score;
    r.kind_label = "contact";
    r.category = "contact";
    out.push_back(std::move(r));
  }
  return out;
}

std::vector<SearchResult> NotesProvider::query(const std::string& text, const Config& cfg,
                                               std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.sources.notes || limit == 0) return out;
  auto needle = fold_search(normalize_query(text));
  if (needle.size() < 2) return out;
  auto roots = roots_or(cfg.sources.notes_paths, default_notes_roots(), nullptr);
  if (roots.empty()) return out;
  std::vector<std::string> files;
  // Collect note files (md/txt/org/markdown) with bounded walk.
  for (auto& r : roots) {
    if (files.size() > 500) break;
    std::error_code ec;
    std::vector<std::pair<std::string, int>> stack{{r, 0}};
    while (!stack.empty() && files.size() < 500) {
      auto [cur, d] = stack.back();
      stack.pop_back();
      std::error_code ec2;
      auto st = fs::status(fs::u8path(cur), ec2);
      if (ec2) continue;
      if (fs::is_regular_file(st)) {
        auto e = to_lower_utf8(path_extension(cur));
        if (e == ".md" || e == ".markdown" || e == ".txt" || e == ".org" || e == ".note")
          files.push_back(cur);
        continue;
      }
      if (!fs::is_directory(st) || d > 4) continue;
      for (auto it = fs::directory_iterator(fs::u8path(cur), ec2);
           it != fs::directory_iterator(); it.increment(ec2)) {
        if (ec2) break;
        stack.emplace_back(it->path().string(), d + 1);
      }
    }
  }
  int budget = (int)std::min<std::size_t>(limit, (std::size_t)cfg.sources.max_results);
  struct Hit {
    int score;
    std::size_t idx;
  };
  std::vector<Note> notes;
  std::vector<Hit> hits;
  for (auto& f : files) {
    std::string raw;
    if (!read_file_all(f, raw)) continue;
    if (looks_binary(std::string_view(raw.data(), std::min<std::size_t>(raw.size(), 8000)))) continue;
    if (raw.size() > 256 * 1024) raw.resize(256 * 1024);
    std::string title = path_stem(f);
    // First Markdown heading wins for the title.
    auto hpos = raw.find("# ");
    if (hpos != std::string::npos) {
      auto nl = raw.find('\n', hpos);
      auto t = raw.substr(hpos + 2, nl == std::string::npos ? std::string::npos : nl - hpos - 2);
      while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
      while (!t.empty() && (t.back() == ' ' || t.back() == '\r')) t.pop_back();
      if (!t.empty() && t.size() < 120) title = t;
    }
    int s = 0;
    std::string hay = title + " " + raw.substr(0, 4000);
    if (!score_hit(needle, hay, s)) continue;
    Note n{title, raw.substr(0, 160), f};
    notes.push_back(std::move(n));
    hits.push_back({s, notes.size() - 1});
  }
  std::sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.score > b.score; });
  for (auto& h : hits) {
    if ((int)out.size() >= budget) break;
    auto& n = notes[h.idx];
    SearchResult r;
    r.title = n.title;
    r.subtitle = path_parent(n.path) + " · Note";
    r.path = n.path;
    r.payload = n.path;
    r.action = ResultAction::Open;
    r.score = h.score;
    r.kind_label = "note";
    r.category = "note";
    out.push_back(std::move(r));
  }
  return out;
}

}  // namespace wilfred
