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

bool native_wifi_status(bool&, std::string&, std::string& error) {
  error = "Wi-Fi control is not available on Android (use system settings)";
  return false;
}
bool native_wifi_set(bool, std::string& error) {
  error = "Wi-Fi control is not available on Android (use system settings)";
  return false;
}
std::vector<std::string> native_wifi_list(std::string& error) {
  error = "Wi-Fi scan is not available on Android";
  return {};
}
bool native_bluetooth_status(bool&, std::string&, std::string& error) {
  error = "Bluetooth control is not available on Android (use system settings)";
  return false;
}
bool native_bluetooth_set(bool, std::string& error) {
  error = "Bluetooth control is not available on Android (use system settings)";
  return false;
}
bool native_volume_status(int&, bool&, std::string& error) {
  error = "volume is handled by the Android system";
  return false;
}
bool native_volume_set(int, std::string& error) {
  error = "volume is handled by the Android system";
  return false;
}
bool native_volume_mute(bool, std::string& error) {
  error = "volume is handled by the Android system";
  return false;
}
bool native_brightness_status(int&, std::string& error) {
  error = "brightness is handled by the Android system";
  return false;
}
bool native_brightness_set(int, std::string& error) {
  error = "brightness is handled by the Android system";
  return false;
}
bool native_open_settings(const std::string&, std::string& error) {
  error = "settings are handled by the Android system";
  return false;
}

#endif
}  // namespace wilfred
