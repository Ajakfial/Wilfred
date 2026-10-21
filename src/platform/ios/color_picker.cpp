#include "wilfred/platform/native.hpp"

#include <string>

namespace wilfred {

// iOS sandboxes app pixels. Reading another app's screen needs a ReplayKit
// broadcast session with user consent, which a launcher cannot start on demand.
bool native_color_at_point(int, int, NativeColor&, std::string& error) {
  error = "Screen color sampling is not available on iOS";
  return false;
}

bool native_color_at_cursor(NativeColor&, std::string& error) {
  error = "Screen color sampling is not available on iOS";
  return false;
}

}  // namespace wilfred
