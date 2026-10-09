#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"

#ifdef __APPLE__
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>

namespace wilfred {
namespace fs = std::filesystem;

namespace {

std::string screenshot_dir_mac() {
  return path_join(path_join(home_directory(), "Pictures"), "Wilfred");
}

std::string next_path_mac(const std::string& dir, const char* ext) {
  std::error_code ec;
  fs::create_directories(fs::u8path(dir), ec);
  std::time_t t = std::time(nullptr);
  std::tm tmv{};
  localtime_r(&t, &tmv);
  char base[64];
  std::strftime(base, sizeof(base), "Screenshot-%Y%m%d-%H%M%S", &tmv);
  for (int i = 0; i < 100; ++i) {
    std::string name = base;
    if (i) name += "-" + std::to_string(i + 1);
    name += ext;
    auto full = path_join(dir, name);
    if (!fs::exists(fs::u8path(full), ec)) return full;
  }
  return path_join(dir, std::string(base) + ext);
}

std::string shell_quote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') o.push_back('\\');
    o.push_back(c);
  }
  o.push_back('"');
  return o;
}

}  // namespace

std::string native_screenshot_save_directory() {
  return screenshot_dir_mac();
}

bool native_take_screenshot(NativeScreenshotMode mode, std::string& out_path, std::string& error) {
  out_path.clear();
  error.clear();
  auto path = next_path_mac(screenshot_dir_mac(), ".png");
  std::string cmd = "screencapture ";
  switch (mode) {
    case NativeScreenshotMode::Fullscreen:
      cmd += "-x ";
      break;
    case NativeScreenshotMode::Window:
      cmd += "-w -x ";
      break;
    case NativeScreenshotMode::Region:
      cmd += "-i -x ";
      break;
  }
  cmd += shell_quote(path);
  cmd += " >/dev/null 2>&1";
  int rc = std::system(cmd.c_str());
  std::error_code ec;
  if (rc == 0 && fs::exists(fs::u8path(path), ec)) {
    out_path = path;
    return true;
  }
  error = "screencapture failed (grant Screen Recording permission in System Settings, then retry)";
  return false;
}

}  // namespace wilfred
#else
namespace wilfred {
std::string native_screenshot_save_directory();
bool native_take_screenshot(NativeScreenshotMode, std::string&, std::string&);
}  // namespace wilfred
#endif
