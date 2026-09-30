#pragma once

#include <string>
#include <string_view>

namespace wilfred {

struct MathResult {
  double value{0};
  std::string display;
  bool ok{false};
  bool conversion{false};
  bool currency{false};
  std::string error;
};

MathResult evaluate_math(std::string_view expr);
bool convert_metric(std::string_view expr, MathResult& out);
bool convert_currency(double amount, std::string_view from, std::string_view to, MathResult& out);
void set_currency_network_enabled(bool enabled);

}  // namespace wilfred
