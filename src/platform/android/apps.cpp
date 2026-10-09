#include "wilfred/platform/native.hpp"

namespace wilfred {
#if defined(__ANDROID__)

// On Android the launcher UI (Kotlin) enumerates installed packages via
// PackageManager and feeds them into the index through
// MobileCore::register_app. The native side therefore reports no
// statically-discovered apps; this keeps the C++ core portable and avoids
// depending on JNI from every call site.

std::vector<AppInfo> native_discover_apps() {
  return {};
}

#endif
}  // namespace wilfred
