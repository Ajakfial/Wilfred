#include "wilfred/search/snippets.hpp"

#include "wilfred/config/yaml.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/fuzzy.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <ctime>
#include <sstream>

namespace wilfred {

std::string default_snippets_path() { return path_join(config_directory(), "snippets.yml"); }

bool snippet_query_forced(const std::string& query, const Config& cfg) {
  auto raw = query;
  auto prefix = cfg.snippets.prefix.empty() ? ";" : cfg.snippets.prefix;
  if (!prefix.empty() && raw.rfind(prefix, 0) == 0) return true;
  auto lq = to_lower_utf8(normalize_query(raw));
  return lq.rfind("snip ", 0) == 0 || lq.rfind("snippet ", 0) == 0 || lq.rfind("snip:", 0) == 0 ||
         lq.rfind("snippet:", 0) == 0 || lq == "snip" || lq == "snippets" || lq == "snippers";
}

bool query_is_snippet_save(const std::string& query, std::string& name_out) {
  auto q = normalize_query(query);
  auto l = to_lower_utf8(q);
  const char* prefixes[] = {"clip save ", "snip save ", "snippet save ", "clip:save ", "snip:save "};
  for (auto* p : prefixes) {
    auto n = std::strlen(p);
    if (l.rfind(p, 0) == 0) {
      name_out = q.substr(n);
      while (!name_out.empty() && name_out.front() == ' ') name_out.erase(name_out.begin());
      return !name_out.empty();
    }
  }
  return false;
}

static Snippet snippet_from_yaml(const std::string& key, const YamlValue& v) {
  Snippet s;
  s.id = key;
  s.trigger = key;
  s.title = key;
  s.kind = "text";
  if (v.is_string()) {
    s.body = v.as_string();
    s.title = key;
    return s;
  }
  if (!v.is_map()) return s;
  s.trigger = v.str("trigger", key);
  s.title = v.str("title", key);
  s.body = v.str("body", v.str("text", ""));
  s.kind = v.str("kind", "text");
  s.id = v.str("id", key);
  s.folder = v.str("folder", v.str("group", ""));
  return s;
}

bool SnippetStore::load(const std::string& path, const Config& cfg) {
  path_ = path.empty() ? default_snippets_path() : path;
  items_.clear();
  for (auto& [k, body] : cfg.snippets.items) {
    Snippet s;
    s.id = k;
    s.trigger = k;
    s.title = k;
    s.body = body;
    s.kind = "text";
    items_.push_back(std::move(s));
  }
  std::string text;
  if (file_exists(path_) && read_file_all(path_, text)) {
    YamlValue root;
    YamlError ye;
    if (parse_yaml(text, root, ye) && root.is_map()) {
      for (auto& [k, v] : root.as_map()) {
        auto s = snippet_from_yaml(k, v);
        if (s.body.empty() && s.trigger.empty()) continue;
        bool replaced = false;
        for (auto& e : items_) {
          if (to_lower_utf8(e.id) == to_lower_utf8(s.id) ||
              to_lower_utf8(e.trigger) == to_lower_utf8(s.trigger)) {
            e = s;
            replaced = true;
            break;
          }
        }
        if (!replaced) items_.push_back(std::move(s));
      }
    }
  }
  return true;
}

bool SnippetStore::save() const {
  if (path_.empty()) return false;
  std::ostringstream os;
  for (auto& s : items_) {
    os << s.id << ":\n";
    os << "  trigger: " << s.trigger << "\n";
    os << "  title: " << s.title << "\n";
    os << "  kind: " << (s.kind.empty() ? "text" : s.kind) << "\n";
    if (!s.folder.empty()) os << "  folder: " << s.folder << "\n";
    os << "  body: |\n";
    std::istringstream body(s.body);
    std::string line;
    while (std::getline(body, line)) os << "    " << line << "\n";
    if (s.body.empty()) os << "    \n";
  }
  auto t = os.str();
  create_directories(path_parent(path_));
  return write_file_atomic(path_, t.data(), t.size());
}

void SnippetStore::upsert(Snippet s) {
  if (s.id.empty()) s.id = s.trigger;
  if (s.trigger.empty()) s.trigger = s.id;
  if (s.title.empty()) s.title = s.trigger;
  auto idl = to_lower_utf8(s.id);
  auto trl = to_lower_utf8(s.trigger);
  for (auto& e : items_) {
    if (to_lower_utf8(e.id) == idl || to_lower_utf8(e.trigger) == trl) {
      e = std::move(s);
      return;
    }
  }
  items_.push_back(std::move(s));
}

bool SnippetStore::remove(const std::string& id) {
  auto l = to_lower_utf8(id);
  auto n = items_.size();
  items_.erase(std::remove_if(items_.begin(), items_.end(),
                              [&](const Snippet& s) {
                                return to_lower_utf8(s.id) == l || to_lower_utf8(s.trigger) == l;
                              }),
               items_.end());
  return items_.size() != n;
}

const Snippet* SnippetStore::find_trigger(const std::string& trigger) const {
  auto want = to_lower_utf8(trigger);
  for (auto& s : items_)
    if (to_lower_utf8(s.trigger) == want) return &s;
  return nullptr;
}

std::string expand_snippet_placeholders(const std::string& body, const std::string& query_text,
                                        const std::string& clipboard_text) {
  std::time_t now = std::time(nullptr);
  std::tm tmv{};
#ifdef _WIN32
  localtime_s(&tmv, &now);
#else
  localtime_r(&now, &tmv);
#endif
  char date[16]{}, time_s[16]{}, datetime[32]{};
  std::strftime(date, sizeof(date), "%Y-%m-%d", &tmv);
  std::strftime(time_s, sizeof(time_s), "%H:%M", &tmv);
  std::strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M", &tmv);
  char year[8]{}, month[8]{}, day[8]{};
  std::strftime(year, sizeof(year), "%Y", &tmv);
  std::strftime(month, sizeof(month), "%m", &tmv);
  std::strftime(day, sizeof(day), "%d", &tmv);
  auto rep = [&](std::string s, const char* from, const std::string& to) {
    std::string f(from);
    std::size_t p = 0;
    while ((p = s.find(f, p)) != std::string::npos) {
      s.replace(p, f.size(), to);
      p += to.size();
    }
    return s;
  };
  std::string o = body;
  o = rep(o, "{date}", date);
  o = rep(o, "{time}", time_s);
  o = rep(o, "{datetime}", datetime);
  o = rep(o, "{year}", year);
  o = rep(o, "{month}", month);
  o = rep(o, "{day}", day);
  o = rep(o, "{clipboard}", clipboard_text);
  o = rep(o, "{query}", query_text);
  return o;
}

const Snippet* snippet_global_match(const SnippetStore& store, const std::string& typed,
                                    std::size_t& trigger_len_out) {
  trigger_len_out = 0;
  if (typed.empty()) return nullptr;
  // Strip one trailing delimiter; the char before it must end a trigger.
  char last = typed.back();
  bool delimited = last == ' ' || last == '\t' || last == '\n' || last == '.' || last == ',' ||
                   last == ';' || last == ':' || last == '!' || last == '?';
  std::string core = delimited ? typed.substr(0, typed.size() - 1) : typed;
  // Take the last whitespace-delimited token as the abbreviation candidate.
  auto sp = core.find_last_of(" \t\n\r");
  std::string tok = sp == std::string::npos ? core : core.substr(sp + 1);
  if (tok.empty() || tok.size() > 64) return nullptr;
  const Snippet* s = store.find_trigger(tok);
  if (!s) return nullptr;
  // Triggers shorter than 2 chars would misfire constantly.
  if (tok.size() < 2) return nullptr;
  trigger_len_out = tok.size();
  return s;
}

std::vector<SearchResult> SnippetStore::match(const std::string& query, const Config& cfg) const {
  std::vector<SearchResult> out;
  if (!cfg.snippets.expansion) return out;
  auto raw = query;
  auto prefix = cfg.snippets.prefix.empty() ? ";" : cfg.snippets.prefix;
  bool forced = false;
  auto lq = to_lower_utf8(normalize_query(raw));
  if (!prefix.empty() && raw.rfind(prefix, 0) == 0) {
    forced = true;
    raw = raw.substr(prefix.size());
    lq = to_lower_utf8(normalize_query(raw));
  } else if (lq.rfind("snip ", 0) == 0 || lq.rfind("snippet ", 0) == 0) {
    forced = true;
    auto sp = raw.find(' ');
    raw = sp == std::string::npos ? std::string() : raw.substr(sp + 1);
    lq = to_lower_utf8(normalize_query(raw));
  } else if (lq.rfind("snip:", 0) == 0 || lq.rfind("snippet:", 0) == 0) {
    forced = true;
    auto sp = raw.find(':');
    raw = sp == std::string::npos ? std::string() : raw.substr(sp + 1);
    lq = to_lower_utf8(normalize_query(raw));
  } else if (lq == "snip" || lq == "snippets" || lq == "snippers") {
    forced = true;
    lq.clear();
  }
  // Folder filter: "folder/name" narrows to one folder.
  std::string folder_filter;
  auto slash = lq.find('/');
  if (forced && slash != std::string::npos && slash > 0 && slash + 1 < lq.size()) {
    folder_filter = lq.substr(0, slash);
    lq = lq.substr(slash + 1);
  }
  ClipboardSnapshot clip;
  bool clip_loaded = false;
  for (auto& s : items_) {
    if (!folder_filter.empty() && to_lower_utf8(s.folder) != folder_filter) continue;
    auto trig = to_lower_utf8(s.trigger);
    // Also match "folder/trigger" text.
    std::string trig_full = trig;
    std::string folder_l = to_lower_utf8(s.folder);
    if (!folder_l.empty()) trig_full = folder_l + "/" + trig;
    int score = 0;
    if (lq.empty() && forced)
      score = 8200;
    else if (trig == lq || trig_full == lq)
      score = 9800;
    else if (!lq.empty() && (trig.rfind(lq, 0) == 0 || trig_full.rfind(lq, 0) == 0))
      score = 9000;
    else if (!lq.empty() && (trig.find(lq) != std::string::npos || trig_full.find(lq) != std::string::npos))
      score = 7600;
    else if (!lq.empty() && score_fuzzy(lq, trig, s.trigger).matched)
      score = 6200 + score_fuzzy(lq, trig, s.trigger).score;
    else if (!forced)
      continue;
    else
      continue;
    if (!clip_loaded && cfg.search.clipboard) {
      clip = read_clipboard();
      clip_loaded = true;
    }
    SearchResult r;
    r.title = s.title.empty() ? s.trigger : s.title;
    std::string where = s.folder.empty() ? s.trigger : s.folder + " / " + s.trigger;
    r.subtitle = (s.kind == "clip" ? "Saved clip · " : "Snippet · ") + where + " · enter pastes";
    // Expand placeholders eagerly so pasting is WYSIWYG.
    r.payload = expand_snippet_placeholders(s.body, query, clip.text);
    r.path = s.trigger;
    r.action = ResultAction::Expand;
    r.score = score;
    r.kind_label = s.kind == "clip" ? "clip" : "snippet";
    r.category = "snippet";
    out.push_back(std::move(r));
  }
  std::sort(out.begin(), out.end(), [](const SearchResult& a, const SearchResult& b) {
    return a.score > b.score;
  });
  return out;
}

}  // namespace wilfred
