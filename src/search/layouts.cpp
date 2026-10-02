#include "wilfred/search/layouts.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

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

}  // namespace wilfred
