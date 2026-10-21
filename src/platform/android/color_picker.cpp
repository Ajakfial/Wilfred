#include "wilfred/platform/native.hpp"

#include <string>

namespace wilfred {

// Android has no API to read other apps' pixels outside a MediaProjection
// session, which requires a per-session consent dialog.
bool native_color_at_point(int, int, NativeColor&, std::string& error) {
  error = "Screen color sampling is not available on Android";
  return false;
}

bool native_color_at_cursor(NativeColor&, std::string& error) {
  error = "Screen color sampling is not available on Android";
  return false;
}

}  // namespace wilfred
