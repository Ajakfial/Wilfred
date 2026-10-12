// `wilfred autostart`: launch-at-login management.
#include "wilfred/service/autostart.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/native.hpp"

#include <iostream>

namespace wilfred {

int run_autostart(const std::vector<std::string>& args) {
  std::string sub = args.empty() ? "status" : to_lower_utf8(args[0]);
  if (sub == "status" || sub == "show" || sub == "st") {
    bool enabled = false;
    std::string detail, error;
    if (!native_autostart_status(enabled, detail, error)) {
      std::cerr << (error.empty() ? "autostart status failed" : error) << "\n";
      return 1;
    }
    std::cout << (enabled ? "enabled" : "disabled");
    if (!detail.empty()) std::cout << " (" << detail << ")";
    std::cout << "\n";
    return 0;
  }
  if (sub == "enable" || sub == "on" || sub == "1") {
    std::string error;
    if (!native_autostart_set(true, error)) {
      std::cerr << (error.empty() ? "autostart enable failed" : error) << "\n";
      return 1;
    }
    std::cout << "autostart enabled\n";
    return 0;
  }
  if (sub == "disable" || sub == "off" || sub == "0") {
    std::string error;
    if (!native_autostart_set(false, error)) {
      std::cerr << (error.empty() ? "autostart disable failed" : error) << "\n";
      return 1;
    }
    std::cout << "autostart disabled\n";
    return 0;
  }
  std::cerr << "usage: wilfred autostart [status|enable|disable]\n";
  return 2;
}

}  // namespace wilfred
