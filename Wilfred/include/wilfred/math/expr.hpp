#pragma once

#include <string>
#include <string_view>

namespace wilfred {

struct MathResult {
  double value{0};
  std::string display;
  bool ok{false};
  bool conversion{false};
  std::string error;
};

MathResult evaluate_math(std::string_view expr);
bool convert_metric(std::string_view expr, MathResult& out);

}  // namespace wilfred
