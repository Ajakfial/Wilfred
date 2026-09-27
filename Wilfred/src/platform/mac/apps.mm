#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"

#import <Foundation/Foundation.h>
#include <filesystem>

namespace wilfred {
#ifdef __APPLE__
namespace fs = std::filesystem;

static void scan_apps_dir(const std::string& dir, std::vector<AppInfo>& out, bool system) {
  std::error_code ec;
  if (!fs::exists(fs::u8path(dir), ec)) return;
  fs::directory_iterator it(fs::u8path(dir), fs::directory_options::skip_permission_denied, ec);
  fs::directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    auto p = it->path();
    if (p.extension() != ".app") continue;
    AppInfo a;
    auto stem = p.stem().u8string();
    a.name = std::string(stem.begin(), stem.end());
    auto u8 = p.u8string();
    a.path = std::string(u8.begin(), u8.end());
    a.identifier = a.name;
    a.system = system;
    out.push_back(std::move(a));
  }
}

std::vector<AppInfo> native_discover_apps() {
  std::vector<AppInfo> apps;
  scan_apps_dir("/Applications", apps, true);
  scan_apps_dir("/System/Applications", apps, true);
  scan_apps_dir(path_join(home_directory(), "Applications"), apps, false);
  return apps;
}

#endif
}  // namespace wilfred
