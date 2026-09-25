#include "wilfred/platform/native.hpp"

#include <cstdlib>
#include <string>

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)

static bool sys(const std::string& cmd) { return std::system(cmd.c_str()) == 0; }

bool native_launch(const std::string& path) {
  std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_reveal(const std::string& path) {
  std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_open_url(const std::string& url) {
  std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

#endif
}  // namespace wilfred
