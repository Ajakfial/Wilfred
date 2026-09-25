#include "wilfred/search/filter.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/classify.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <cctype>
#include <sstream>

namespace wilfred {

bool record_matches_filter(const SearchFilter& f, const IndexStore& store,
                           const IndexRecord& rec) {
  auto path = std::string(store.pool().get(rec.path_id));
  auto name = std::string(store.pool().get(rec.name_id));
  auto ext = std::string(store.pool().get(rec.ext_id));

  if (!f.extensions.empty()) {
    bool ok = false;
    auto el = fold_search(ext);
    for (auto& e : f.extensions) {
      auto want = fold_search(e);
      if (!want.empty() && want[0] != '.') want.insert(want.begin(), '.');
      if (el == want) {
        ok = true;
        break;
      }
    }
    if (!ok) return false;
  }
  if (!f.kinds.empty()) {
    bool ok = false;
    for (auto k : f.kinds)
      if (k == rec.kind) {
        ok = true;
        break;
      }
    if (!ok) return false;
  }
  if (!f.in_dirs.empty()) {
    bool ok = false;
    for (auto& d : f.in_dirs)
      if (path_is_under(path, d) || contains_ci(path, d)) {
        ok = true;
        break;
      }
    if (!ok) return false;
  }
  for (auto& n : f.name_contains)
    if (!contains_ci(name, n)) return false;
  if (f.min_size && rec.size < *f.min_size) return false;
  if (f.max_size && rec.size > *f.max_size) return false;
  if (f.min_mtime && rec.mtime < *f.min_mtime) return false;
  if (f.max_mtime && rec.mtime > *f.max_mtime) return false;
  if (f.hidden && has_flag(rec.flags, RecordFlags::Hidden) != *f.hidden) return false;
  if (f.system && has_flag(rec.flags, RecordFlags::System) != *f.system) return false;
  if (f.directories_only && *f.directories_only && !has_flag(rec.flags, RecordFlags::Directory))
    return false;
  if (f.files_only && *f.files_only && has_flag(rec.flags, RecordFlags::Directory)) return false;
  if (f.apps_only && *f.apps_only && rec.kind != FileKind::Application) return false;
  return true;
}

static std::vector<std::string> split_ws(const std::string& s) {
  std::vector<std::string> o;
  std::string cur;
  for (char c : s) {
    if (c == ' ' || c == '\t') {
      if (!cur.empty()) {
        o.push_back(cur);
        cur.clear();
      }
    } else
      cur.push_back(c);
  }
  if (!cur.empty()) o.push_back(cur);
  return o;
}

std::uint64_t parse_size_token(std::string_view t, bool& ok) {
  ok = false;
  std::string s(t);
  if (s.empty()) return 0;
  char last = static_cast<char>(std::tolower(static_cast<unsigned char>(s.back())));
  double mul = 1;
  if (last == 'k' || (s.size() > 1 && (s.back() == 'b' || s.back() == 'B'))) {
    auto l = to_lower_utf8(s);
    if (l.size() >= 2 && l.substr(l.size() - 2) == "kb") {
      mul = 1024;
      s = s.substr(0, s.size() - 2);
    } else if (l.size() >= 2 && l.substr(l.size() - 2) == "mb") {
      mul = 1024 * 1024;
      s = s.substr(0, s.size() - 2);
    } else if (l.size() >= 2 && l.substr(l.size() - 2) == "gb") {
      mul = 1024.0 * 1024 * 1024;
      s = s.substr(0, s.size() - 2);
    } else if (last == 'k') {
      mul = 1024;
      s.pop_back();
    } else if (last == 'm') {
      mul = 1024 * 1024;
      s.pop_back();
    } else if (last == 'g') {
      mul = 1024.0 * 1024 * 1024;
      s.pop_back();
    }
  } else if (last == 'm' || last == 'g') {
    mul = last == 'm' ? 1024 * 1024 : 1024.0 * 1024 * 1024;
    s.pop_back();
  }
  try {
    auto v = std::stod(s);
    ok = true;
    return static_cast<std::uint64_t>(v * mul);
  } catch (...) {
    return 0;
  }
}

std::int64_t parse_time_token(std::string_view t, bool& ok) {
  ok = false;
  std::string s = to_lower_utf8(t);
  auto now = unix_seconds();
  if (s == "today" || s == "recent" || s == "recently") {
    ok = true;
    return now - 7 * 86400;
  }
  if (s.empty()) return 0;
  char u = s.back();
  std::string num = s;
  std::int64_t mul = 1;
  if (u == 'd') {
    mul = 86400;
    num.pop_back();
  } else if (u == 'h') {
    mul = 3600;
    num.pop_back();
  } else if (u == 'w') {
    mul = 86400 * 7;
    num.pop_back();
  } else if (u == 'm') {
    mul = 86400 * 30;
    num.pop_back();
  }
  try {
    auto v = std::stoll(num);
    ok = true;
    return now - v * mul;
  } catch (...) {
    return 0;
  }
}

SearchFilter parse_filter_clauses(std::string& query_inout) {
  SearchFilter f;
  auto parts = split_ws(query_inout);
  std::string leftover;
  leftover.reserve(query_inout.size());
  for (std::size_t i = 0; i < parts.size(); ++i) {
    auto& p = parts[i];
    auto colon = p.find(':');
    if (colon != std::string::npos && colon > 0) {
      auto key = to_lower_utf8(p.substr(0, colon));
      auto val = p.substr(colon + 1);
      if (key == "ext" || key == "extension") {
        f.extensions.push_back(val);
        continue;
      }
      if (key == "type" || key == "kind") {
        auto k = kind_from_name(val);
        if (k != FileKind::Unknown) f.kinds.push_back(k);
        if (to_lower_utf8(val) == "app" || to_lower_utf8(val) == "apps") f.apps_only = true;
        continue;
      }
      if (key == "in" || key == "path" || key == "dir") {
        f.in_dirs.push_back(val);
        continue;
      }
      if (key == "scope") {
        f.scope_names.push_back(val);
        continue;
      }
      if (key == "name") {
        f.name_contains.push_back(val);
        continue;
      }
      if (key == "size") {
        bool ok = false;
        if (!val.empty() && val[0] == '>') {
          f.min_size = parse_size_token(val.substr(1), ok);
        } else if (!val.empty() && val[0] == '<') {
          f.max_size = parse_size_token(val.substr(1), ok);
        } else {
          f.min_size = parse_size_token(val, ok);
        }
        continue;
      }
      if (key == "modified" || key == "after") {
        bool ok = false;
        f.min_mtime = parse_time_token(val, ok);
        continue;
      }
      if (key == "hidden") {
        f.hidden = to_lower_utf8(val) != "false" && val != "0";
        continue;
      }
      if (key == "system") {
        f.system = to_lower_utf8(val) != "false" && val != "0";
        continue;
      }
    }
    auto pl = to_lower_utf8(p);
    if ((pl == "in" || pl == "inside") && i + 1 < parts.size()) {
      f.in_dirs.push_back(parts[++i]);
      continue;
    }
    if (pl == "containing" && i + 1 < parts.size()) {
      f.name_contains.push_back(parts[++i]);
      continue;
    }
    if (pl.size() >= 2 && pl[0] == '*' && pl[1] == '.') {
      f.extensions.push_back(p.substr(1));
      continue;
    }
    if (pl.size() >= 2 && pl[0] == '.') {
      auto k = classify_extension(pl);
      if (k != FileKind::File) {
        f.extensions.push_back(pl);
        continue;
      }
    }
    {
      auto dotted = pl[0] == '.' ? pl : std::string(".") + pl;
      auto k = classify_extension(dotted);
      if (k != FileKind::File && k != FileKind::Unknown && pl.find('/') == std::string::npos &&
          pl.find('\\') == std::string::npos && pl.size() <= 8) {
        bool likely_ext = true;
        for (char c : pl)
          if (c != '.' && !std::isalnum(static_cast<unsigned char>(c))) likely_ext = false;
        if (likely_ext && (pl == "pdf" || pl == "png" || pl == "jpg" || pl == "cpp" || pl == "h" ||
                           pl == "hpp" || pl == "rs" || pl == "py" || pl == "mp4" || pl == "mp3" ||
                           pl == "zip" || pl == "exe" || pl == "docx" || pl == "txt" || pl == "md")) {
          f.extensions.push_back(dotted);
          continue;
        }
      }
    }
    if (pl == "applications" || pl == "apps") {
      f.apps_only = true;
      f.kinds.push_back(FileKind::Application);
      continue;
    }
    if (pl == "folders" || pl == "directories") {
      f.directories_only = true;
      continue;
    }
    if (pl == "images" || pl == "photos") {
      f.kinds.push_back(FileKind::Image);
      continue;
    }
    if (pl == "videos" || pl == "movies") {
      f.kinds.push_back(FileKind::Video);
      continue;
    }
    if (pl == "documents" || pl == "docs") {
      f.kinds.push_back(FileKind::Document);
      continue;
    }
    if (pl == "music" || pl == "audio") {
      f.kinds.push_back(FileKind::Audio);
      continue;
    }
    if (pl == "executables") {
      f.kinds.push_back(FileKind::Executable);
      continue;
    }
    if (pl == "hidden") {
      f.hidden = true;
      continue;
    }
    if (pl == "system") {
      f.system = true;
      continue;
    }
    if (pl == "large") {
      f.min_size = 10ull * 1024 * 1024;
      continue;
    }
    if (pl == "recent" || pl == "recently") {
      f.min_mtime = unix_seconds() - 7 * 86400;
      continue;
    }
    if (pl == "modified") continue;
    if (pl == "files") continue;
    leftover += leftover.empty() ? p : " " + p;
  }
  query_inout = leftover;
  return f;
}

void apply_named_scopes(SearchFilter& f, const Config& cfg) {
  for (auto& name : f.scope_names) {
    auto it = cfg.scopes.find(to_lower_utf8(name));
    if (it == cfg.scopes.end()) continue;
    for (auto& p : it->second) f.in_dirs.push_back(p);
  }
}

}  // namespace wilfred
