#include "wilfred/search/layouts.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/native.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <sstream>

namespace wilfred {

namespace fs = std::filesystem;

LayoutStore& LayoutStore::instance() {
  static LayoutStore inst;
  return inst;
}

void LayoutStore::configure(std::string dir) {
  std::lock_guard<std::mutex> lock(mu_);
  dir_ = std::move(dir);
}

bool LayoutStore::valid_name(const std::string& name) {
  if (name.empty() || name.size() > 64) return false;
  for (char c : name) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-';
    if (!ok) return false;
  }
  return true;
}

static std::string layout_file(const std::string& dir, const std::string& name) {
  return path_join(dir, to_lower_utf8(name) + ".json");
}

std::string LayoutStore::serialize(const Layout& layout) {
  std::ostringstream os;
  os << "{\"name\":\"" << json_escape(layout.name) << "\",\"windows\":[";
  for (std::size_t i = 0; i < layout.entries.size(); ++i) {
    auto& e = layout.entries[i];
    if (i) os << ",";
    os << "{\"match\":\"" << json_escape(e.match) << "\",\"x\":" << e.x << ",\"y\":" << e.y
       << ",\"w\":" << e.w << ",\"h\":" << e.h
       << ",\"maximized\":" << (e.maximized ? "true" : "false") << "}";
  }
  os << "]}";
  return os.str();
}

bool LayoutStore::parse(const std::string& blob, Layout& out) {
  Layout lay;
  lay.name = json_get_string(blob, "name");
  if (lay.name.empty()) return false;
  auto arr = json_extract_array(blob, "windows");
  if (arr.empty()) return false;
  for (auto& obj : json_object_array(arr)) {
    LayoutEntry e;
    e.match = json_get_string(obj, "match");
    if (e.match.empty()) return false;
    e.x = json_get_int(obj, "x", 0);
    e.y = json_get_int(obj, "y", 0);
    e.w = json_get_int(obj, "w", 0);
    e.h = json_get_int(obj, "h", 0);
    e.maximized = json_get_bool(obj, "maximized", false);
    lay.entries.push_back(std::move(e));
    if (lay.entries.size() > 64) return false;
  }
  if (lay.entries.empty()) return false;
  out = std::move(lay);
  return true;
}

bool LayoutStore::save_layout(const Layout& layout, std::string& error) {
  error.clear();
  if (!valid_name(layout.name)) {
    error = "layout names use letters, digits, _ and -";
    return false;
  }
  if (layout.entries.empty() || layout.entries.size() > 64) {
    error = "layout needs 1..64 window entries";
    return false;
  }
  std::string dir;
  {
    std::lock_guard<std::mutex> lock(mu_);
    dir = dir_;
  }
  if (dir.empty()) {
    error = "layouts directory is not configured";
    return false;
  }
  create_directories(dir);
  auto body = serialize(layout);
  if (!write_file_atomic(layout_file(dir, layout.name), body.data(), body.size())) {
    error = "could not save layout";
    return false;
  }
  return true;
}

bool LayoutStore::load_layout(const std::string& name, Layout& out, std::string& error) const {
  error.clear();
  if (!valid_name(name)) {
    error = "layout names use letters, digits, _ and -";
    return false;
  }
  std::string dir;
  {
    std::lock_guard<std::mutex> lock(mu_);
    dir = dir_;
  }
  if (dir.empty()) {
    error = "layouts directory is not configured";
    return false;
  }
  std::string blob;
  if (!read_file_all(layout_file(dir, name), blob) || !parse(blob, out)) {
    error = "no layout named \"" + name + "\" (try layouts to list)";
    return false;
  }
  return true;
}

bool LayoutStore::delete_layout(const std::string& name) {
  if (!valid_name(name)) return false;
  std::string dir;
  {
    std::lock_guard<std::mutex> lock(mu_);
    dir = dir_;
  }
  if (dir.empty()) return false;
  return remove_file(layout_file(dir, name));
}

std::vector<std::string> LayoutStore::list() const {
  std::vector<std::string> out;
  std::string dir;
  {
    std::lock_guard<std::mutex> lock(mu_);
    dir = dir_;
  }
  if (dir.empty()) return out;
  std::error_code ec;
  for (const auto& de : fs::directory_iterator(fs::u8path(dir), ec)) {
    if (ec) break;
    std::error_code ec2;
    if (!de.is_regular_file(ec2)) continue;
    auto ext = de.path().extension().u8string();
    std::string exts(ext.begin(), ext.end());
    if (to_lower_utf8(exts) != ".json") continue;
    auto stem = de.path().stem().u8string();
    out.emplace_back(stem.begin(), stem.end());
  }
  std::sort(out.begin(), out.end());
  return out;
}

std::vector<std::string> LayoutStore::tiling_preset_names() {
  return {"halves", "thirds", "grid", "columns", "rows", "stack"};
}

bool LayoutStore::apply_layout_by_name(const std::string& name, std::string& error) {
  error.clear();
  Layout lay;
  if (!instance().load_layout(to_lower_utf8(name), lay, error)) return false;
  auto wins = native_list_windows();
  std::vector<std::uint64_t> used;
  for (auto& e : lay.entries) {
    auto needle = to_lower_utf8(e.match);
    for (auto& w : wins) {
      bool taken = false;
      for (auto u : used)
        if (u == w.id) {
          taken = true;
          break;
        }
      if (taken) continue;
      auto hay = to_lower_utf8(w.title + " " + w.owner);
      if (hay.find(needle) == std::string::npos) continue;
      used.push_back(w.id);
      if (e.maximized) {
        if (!native_window_action(w.id, NativeWindowOp::Maximize, error)) return false;
      } else if (!native_window_move(w.id, e.x, e.y, e.w, e.h, error)) {
        return false;
      }
      break;
    }
  }
  return true;
}

bool LayoutStore::apply_tiling_preset(const std::string& preset, std::string& error) {
  error.clear();
  auto wins = native_list_windows();
  if (wins.empty()) {
    error = "no open windows to tile";
    return false;
  }
  NativeWorkArea area;
  if (!native_primary_work_area(area, error)) return false;
  // Parse "columns 3" / "rows 2" / bare names.
  std::string p = to_lower_utf8(preset);
  while (!p.empty() && (p.front() == ' ' || p.front() == '\t'))
    p.erase(p.begin());
  while (!p.empty() && (p.back() == ' ' || p.back() == '\t'))
    p.pop_back();
  std::string mode = p;
  int count = 0;
  if (p.rfind("columns", 0) == 0) {
    mode = "columns";
    try {
      count = std::stoi(p.substr(7));
    } catch (...) {
      count = 0;
    }
  } else if (p.rfind("rows", 0) == 0) {
    mode = "rows";
    try {
      count = std::stoi(p.substr(4));
    } catch (...) {
      count = 0;
    }
  }
  std::sort(wins.begin(), wins.end(), [](const NativeWindowInfo& a, const NativeWindowInfo& b) {
    return to_lower_utf8(a.title) < to_lower_utf8(b.title);
  });
  auto move_all = [&](const std::vector<NativeWorkArea>& cells) {
    for (std::size_t i = 0; i < wins.size() && i < cells.size(); ++i) {
      std::string err;
      // Best effort per window; report the first failure.
      if (!native_window_move(wins[i].id, cells[i].x, cells[i].y, cells[i].w, cells[i].h, err)) {
        error = err.empty() ? "could not tile window" : err;
        return false;
      }
    }
    return true;
  };
  if (mode == "halves") {
    int half = area.w / 2;
    std::vector<NativeWorkArea> cells = {{area.x, area.y, half, area.h},
                                         {area.x + half, area.y, area.w - half, area.h}};
    // Extra windows stack onto the right half.
    while (cells.size() < wins.size())
      cells.push_back(cells.back());
    return move_all(cells);
  }
  if (mode == "thirds") {
    int third = area.w / 3;
    std::vector<NativeWorkArea> cells = {{area.x, area.y, third, area.h},
                                         {area.x + third, area.y, third, area.h},
                                         {area.x + 2 * third, area.y, area.w - 2 * third, area.h}};
    while (cells.size() < wins.size())
      cells.push_back(cells.back());
    return move_all(cells);
  }
  if (mode == "grid") {
    std::size_t n = wins.size();
    int cols = n <= 1 ? 1 : n <= 4 ? 2 : 3;
    int rows = static_cast<int>((n + cols - 1) / cols);
    std::vector<NativeWorkArea> cells;
    for (std::size_t i = 0; i < n; ++i) {
      int c = static_cast<int>(i % cols);
      int r = static_cast<int>(i / cols);
      cells.push_back(
          {area.x + c * area.w / cols, area.y + r * area.h / rows, area.w / cols, area.h / rows});
    }
    return move_all(cells);
  }
  if (mode == "columns") {
    int n = count > 0 ? count : static_cast<int>(wins.size());
    if (n < 1) n = 1;
    if (n > 8) n = 8;
    std::vector<NativeWorkArea> cells;
    for (std::size_t i = 0; i < wins.size(); ++i) {
      int c = static_cast<int>(i % static_cast<std::size_t>(n));
      cells.push_back({area.x + c * area.w / n, area.y, area.w / n, area.h});
    }
    return move_all(cells);
  }
  if (mode == "rows") {
    int n = count > 0 ? count : static_cast<int>(wins.size());
    if (n < 1) n = 1;
    if (n > 8) n = 8;
    std::vector<NativeWorkArea> cells;
    for (std::size_t i = 0; i < wins.size(); ++i) {
      int r = static_cast<int>(i % static_cast<std::size_t>(n));
      cells.push_back({area.x, area.y + r * area.h / n, area.w, area.h / n});
    }
    return move_all(cells);
  }
  if (mode == "stack") {
    const int step = 32;
    std::vector<NativeWorkArea> cells;
    for (std::size_t i = 0; i < wins.size(); ++i) {
      int off = static_cast<int>(i) * step;
      int w = area.w - off;
      int h = area.h - off;
      if (w < 400) w = 400;
      if (h < 300) h = 300;
      cells.push_back({area.x + off, area.y + off, w, h});
    }
    return move_all(cells);
  }
  error =
      "unknown tiling preset '" + preset + "' (try halves, thirds, grid, columns N, rows N, stack)";
  return false;
}

}  // namespace wilfred
