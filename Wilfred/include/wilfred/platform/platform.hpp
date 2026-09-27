#pragma once

#include <string>

namespace wilfred {

std::string platform_name();
bool is_windows();
bool is_macos();
bool is_linux();

void platform_init();
void pump_native_events();

}  // namespace wilfred
