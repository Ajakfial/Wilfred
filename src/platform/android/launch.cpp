#include "wilfred/platform/native.hpp"

namespace wilfred {
#if defined(__ANDROID__)

// File/app launching on Android must go through Intents (Kotlin) because it
// needs a Context, FileProvider URIs and per-app task stacks. The JNI search
// results carry path/payload/action back to Kotlin, which performs the
// actual startActivity. These stubs exist so wilfred_core links unchanged.

bool native_launch(const std::string&) { return false; }
bool native_reveal(const std::string&) { return false; }

std::vector<OpenWithApp> native_apps_for_file(const std::string&, std::size_t) { return {}; }
bool native_open_with(const std::string&, const std::string&) { return false; }
bool native_open_terminal(const std::string&) { return false; }
bool native_open_editor(const std::string&) { return false; }

bool native_system_action(const std::string&) {
  // Lock/sleep/shutdown require device-admin privileges on Android;
  // intentionally unsupported.
  return false;
}

bool native_kill_process(std::uint32_t, std::string& error) {
  error = "process control is not available on Android";
  return false;
}

bool native_media_action(const std::string&, std::string& error) {
  error = "media keys are handled by the Android system";
  return false;
}

#endif
}  // namespace wilfred
