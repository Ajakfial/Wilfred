#include "wilfred/platform/native.hpp"

namespace wilfred {
#if defined(__ANDROID__)

// Browser handling on Android is owned by the Kotlin layer (Custom Tabs /
// ACTION_VIEW intents). The native core only classifies URLs and produces
// WebSearch results; launching is performed from Kotlin based on the JNI
// result payload, so these stay as explicit stubs.

std::string native_default_browser_id() {
  return "android-system";
}
std::string native_default_browser_executable() {
  return {};
}
std::vector<BrowserInfo> native_list_browsers() {
  return {};
}
bool native_open_url(const std::string&) {
  // Handled in Kotlin (WilfredBridge.openResult -> ACTION_VIEW).
  return false;
}

#endif
}  // namespace wilfred
