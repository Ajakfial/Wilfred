#include "wilfred/platform/native.hpp"

#if defined(__APPLE__)
#include <string>

namespace wilfred {

// macOS needs ScreenCaptureKit (SCScreenshotManager) to read the framebuffer
// reliably: CGDisplayCreateImage is deprecated as of macOS 14 and returns NULL
// without Screen Recording permission. Until that backend lands this reports
// unsupported rather than guessing at a pixel.
bool native_color_at_point(int, int, NativeColor&, std::string& error) {
  error = "Screen color sampling is not implemented on macOS yet";
  return false;
}

bool native_color_at_cursor(NativeColor&, std::string& error) {
  error = "Screen color sampling is not implemented on macOS yet";
  return false;
}

}  // namespace wilfred
#else
namespace wilfred {
bool native_color_at_point(int, int, NativeColor&, std::string&);
bool native_color_at_cursor(NativeColor&, std::string&);
}  // namespace wilfred
#endif
