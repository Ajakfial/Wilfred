#include "wilfred/platform/native.hpp"

#include <string>

namespace wilfred {

// Login items are intentionally unsupported: the OS manages background apps.
bool native_autostart_status(bool& /*enabled*/, std::string& /*detail*/, std::string& error) {
  error = "autostart is not available on Android (the system manages background apps)";
  return false;
}

bool native_autostart_set(bool /*enable*/, std::string& error) {
  error = "autostart is not available on Android (the system manages background apps)";
  return false;
}

}  // namespace wilfred
