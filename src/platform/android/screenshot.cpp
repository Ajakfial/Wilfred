#include "wilfred/platform/native.hpp"

namespace wilfred {
#if defined(__ANDROID__)

bool native_take_screenshot(NativeScreenshotMode, std::string&, std::string& error) {
  error = "screenshots on Android use MediaProjection from the Kotlin layer";
  return false;
}

std::string native_screenshot_save_directory() { return "/sdcard/Pictures/Wilfred"; }

#endif
}  // namespace wilfred
