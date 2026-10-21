#include "wilfred/platform/native.hpp"

#include <string>

namespace wilfred {

bool native_active_menu_items(std::vector<NativeMenuItem>&, std::string& error) {
  error = "Menu search is desktop-only";
  return false;
}

bool native_invoke_menu_item(const std::string&, std::string& error) {
  error = "Menu search is desktop-only";
  return false;
}

void native_menu_remember_foreground() {}

}  // namespace wilfred
