#include "wilfred/platform/native.hpp"

#if !defined(_WIN32) && !defined(__APPLE__)
#include <string>

namespace wilfred {

// AT-SPI lives behind D-Bus. Until that dependency is added, Linux reports
// unsupported rather than shelling out to a tool that may not be installed.
bool native_active_menu_items(std::vector<NativeMenuItem>&, std::string& error) {
  error = "Menu search is not implemented on Linux yet (needs AT-SPI over D-Bus)";
  return false;
}

bool native_invoke_menu_item(const std::string&, std::string& error) {
  error = "Menu search is not implemented on Linux yet";
  return false;
}

void native_menu_remember_foreground() {}

}  // namespace wilfred
#else
namespace wilfred {
bool native_active_menu_items(std::vector<NativeMenuItem>&, std::string&);
bool native_invoke_menu_item(const std::string&, std::string&);
void native_menu_remember_foreground();
}  // namespace wilfred
#endif
