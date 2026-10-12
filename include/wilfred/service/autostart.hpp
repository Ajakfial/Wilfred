#pragma once

#include <string>
#include <vector>

namespace wilfred {

// `wilfred autostart [status|enable|disable]`: manage launch-at-login.
// Returns the process exit code (0 ok, 1 failure, 2 usage).
int run_autostart(const std::vector<std::string>& args);

}  // namespace wilfred
