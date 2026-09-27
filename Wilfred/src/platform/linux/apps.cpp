#include "wilfred/platform/native.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)
namespace fs = std::filesystem;

static std::string desktop_field(const std::string& text, const char* key) {
  std::string line, prefix = std::string(key) + "=";
  std::istringstream is(text);
  while (std::getline(is, line)) {
    if (line.rfind(prefix, 0) == 0) return line.substr(prefix.size());
  }
  return {};
}

static void scan_desktop_dir(const std::string& dir, std::vector<AppInfo>& out) {
  std::error_code ec;
  if (!fs::exists(fs::u8path(dir), ec)) return;
  fs::directory_iterator it(fs::u8path(dir), ec);
  fs::directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (it->path().extension() != ".desktop") continue;
    std::string text;
    auto u8 = it->path().u8string();
    if (!read_file_all(std::string(u8.begin(), u8.end()), text)) continue;
    AppInfo a;
    a.name = desktop_field(text, "Name");
    auto exec = desktop_field(text, "Exec");
    auto pos = exec.find(' ');
    if (pos != std::string::npos) exec = exec.substr(0, pos);
    a.path = exec;
    if (a.path.empty()) a.path = std::string(u8.begin(), u8.end());
    auto kw = desktop_field(text, "Keywords");
    if (!kw.empty()) a.keywords.push_back(kw);
    if (!a.name.empty()) out.push_back(std::move(a));
  }
}

std::vector<AppInfo> native_discover_apps() {
  std::vector<AppInfo> apps;
  const char* home = std::getenv("HOME");
  scan_desktop_dir("/usr/share/applications", apps);
  scan_desktop_dir("/usr/local/share/applications", apps);
  if (home) {
    scan_desktop_dir(std::string(home) + "/.local/share/applications", apps);
  }
  return apps;
}

#endif
}  // namespace wilfred
