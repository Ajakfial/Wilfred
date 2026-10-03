#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

namespace wilfred {
#if defined(WILFRED_IOS)

// File/app launching on iOS must go through the Swift layer (UIApplication
// for URLs, UIActivityViewController for files) because it needs the app
// lifecycle, document interaction controllers and sandbox entitlements. The
// bridge search results carry path/payload/action back to Swift, which
// performs the actual open. These stubs exist so wilfred_core links
// unchanged.

bool native_launch(const std::string&) { return false; }
bool native_reveal(const std::string&) { return false; }

std::vector<OpenWithApp> native_apps_for_file(const std::string&, std::size_t) { return {}; }
bool native_open_with(const std::string&, const std::string&) { return false; }
bool native_open_terminal(const std::string&) { return false; }
bool native_open_editor(const std::string&) { return false; }

bool native_system_action(const std::string&) {
  // Lock/sleep/shutdown are unavailable to sandboxed iOS apps;
  // intentionally unsupported.
  return false;
}

bool native_kill_process(std::uint32_t, std::string& error) {
  error = "process control is not available on iOS";
  return false;
}

bool native_media_action(const std::string&, std::string& error) {
  error = "media keys are handled by iOS";
  return false;
}

#endif
}  // namespace wilfred
