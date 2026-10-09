#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

namespace wilfred {
#if defined(WILFRED_IOS)

// iOS has no desktop window manager visible to native code; window
// listing/management is intentionally unsupported.

std::vector<NativeWindowInfo> native_list_windows() {
  return {};
}
bool native_focus_window(std::uint64_t) {
  return false;
}

bool native_window_action(std::uint64_t, NativeWindowOp, std::string& error) {
  error = "window management is not available on iOS";
  return false;
}

bool native_window_rect(std::uint64_t, NativeWindowRect&, std::string& error) {
  error = "window management is not available on iOS";
  return false;
}

bool native_window_move(std::uint64_t, int, int, int, int, std::string& error) {
  error = "window management is not available on iOS";
  return false;
}

#endif
}  // namespace wilfred
