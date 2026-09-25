#include "wilfred/ui/web_ui.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/record.hpp"

#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#include <cstdlib>
#include <unistd.h>
#else
#include <climits>
#include <unistd.h>
#endif

namespace wilfred {
namespace fs = std::filesystem;

std::string overlay_json_escape(const std::string& s) {
  std::string o;
  o.reserve(s.size() + 8);
  for (unsigned char c : s) {
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else if (c < 0x20)
      continue;
    else
      o.push_back(static_cast<char>(c));
  }
  return o;
}

const char* overlay_action_name(ResultAction a) {
  switch (a) {
    case ResultAction::Reveal:
      return "reveal";
    case ResultAction::Copy:
      return "copy";
    case ResultAction::WebSearch:
      return "web";
    case ResultAction::Calculate:
      return "calc";
    case ResultAction::Convert:
      return "convert";
    case ResultAction::None:
      return "none";
    default:
      return "open";
  }
}

std::string overlay_results_json(const std::vector<SearchResult>& items) {
  std::string o = "{\"type\":\"results\",\"items\":[";
  bool first = true;
  for (auto& it : items) {
    if (!first) o += ',';
    first = false;
    o += "{\"title\":\"";
    o += overlay_json_escape(it.title);
    o += "\",\"subtitle\":\"";
    o += overlay_json_escape(it.subtitle);
    o += "\",\"path\":\"";
    o += overlay_json_escape(it.path);
    o += "\",\"kind\":\"";
    o += overlay_json_escape(std::string(kind_name(it.kind)));
    o += "\",\"action\":\"";
    o += overlay_action_name(it.action);
    o += "\"}";
  }
  o += "]}";
  return o;
}

bool overlay_json_field(const std::string& json, const char* key, std::string& out) {
  std::string k = std::string("\"") + key + "\"";
  auto pos = json.find(k);
  if (pos == std::string::npos) return false;
  pos = json.find(':', pos + k.size());
  if (pos == std::string::npos) return false;
  ++pos;
  while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;
  if (pos >= json.size()) return false;
  if (json[pos] == '"') {
    ++pos;
    out.clear();
    while (pos < json.size() && json[pos] != '"') {
      if (json[pos] == '\\' && pos + 1 < json.size()) {
        out.push_back(json[pos + 1]);
        pos += 2;
      } else
        out.push_back(json[pos++]);
    }
    return true;
  }
  out.clear();
  while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ' ')
    out.push_back(json[pos++]);
  return !out.empty();
}

static std::string exe_directory() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (!n) return ".";
  return path_parent(wide_to_utf8(std::wstring(buf, n)));
#elif defined(__APPLE__)
  char buf[1024];
  uint32_t sz = sizeof(buf);
  if (_NSGetExecutablePath(buf, &sz) != 0) return ".";
  char real[PATH_MAX];
  if (!realpath(buf, real)) return path_parent(buf);
  return path_parent(real);
#else
  char buf[PATH_MAX];
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) return ".";
  buf[n] = 0;
  return path_parent(buf);
#endif
}

std::string overlay_ui_dir() {
  std::error_code ec;
  std::vector<std::string> roots = {exe_directory()};
  auto cwd = fs::current_path(ec);
  if (!ec) roots.push_back(cwd.string());
  auto cur = exe_directory();
  for (int i = 0; i < 4; ++i) {
    cur = path_parent(cur);
    if (cur.empty()) break;
    roots.push_back(cur);
  }
  for (auto& root : roots) {
    if (root.empty()) continue;
    auto dir = path_join(path_join(root, "ui"), "overlay");
    auto html = path_join(dir, "index.html");
    if (fs::exists(fs::u8path(html), ec)) return dir;
  }
  return path_join(path_join(exe_directory(), "ui"), "overlay");
}

}  // namespace wilfred
