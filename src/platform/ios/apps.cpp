#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

namespace wilfred {
#if defined(WILFRED_IOS)

// iOS has no app-enumeration API: the sandbox hides other apps and there is
// no PackageManager equivalent. The Swift layer therefore never calls
// MobileCore::register_app, and the native side reports no
// statically-discovered apps. URLs and files still open from Swift via
// UIApplication / UIActivityViewController based on the result payload.

std::vector<AppInfo> native_discover_apps() {
  return {};
}

#endif
}  // namespace wilfred
