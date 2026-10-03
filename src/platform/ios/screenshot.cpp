#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

#include <cstdlib>
#include <string>

namespace wilfred {
#if defined(WILFRED_IOS)

bool native_take_screenshot(NativeScreenshotMode, std::string&, std::string& error) {
  error = "screenshots on iOS use the system gesture (Side + Volume Up)";
  return false;
}

std::string native_screenshot_save_directory() {
  // Sandbox Documents, mirroring the Android /sdcard/Pictures convention
  // inside the app container.
  std::string home;
  if (const char* h = std::getenv("HOME"); h && *h) home = h;
  if (home.empty()) home = ".";
  return home + "/Documents/Wilfred";
}

#endif
}  // namespace wilfred
