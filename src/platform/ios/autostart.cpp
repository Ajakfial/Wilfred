#include "wilfred/platform/native.hpp"

#include <string>

namespace wilfred {

// Login items are intentionally unsupported: iOS forbids them for store apps.
bool native_autostart_status(bool& /*enabled*/, std::string& /*detail*/, std::string& error) {
  error = "autostart is not available on iOS";
  return false;
}

bool native_autostart_set(bool /*enable*/, std::string& error) {
  error = "autostart is not available on iOS";
  return false;
}

}  // namespace wilfred
