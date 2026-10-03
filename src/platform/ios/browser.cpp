#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

namespace wilfred {
#if defined(WILFRED_IOS)

// Browser handling on iOS is owned by the Swift layer (UIApplication.open
// for http(s) payloads). The native core only classifies URLs and produces
// WebSearch results; launching is performed from Swift based on the bridge
// result payload, so these stay as explicit stubs.

std::string native_default_browser_id() { return "ios-system"; }
std::string native_default_browser_executable() { return {}; }
std::vector<BrowserInfo> native_list_browsers() { return {}; }
bool native_open_url(const std::string&) {
  // Handled in Swift (WilfredActions.openResult -> UIApplication.open).
  return false;
}

#endif
}  // namespace wilfred
