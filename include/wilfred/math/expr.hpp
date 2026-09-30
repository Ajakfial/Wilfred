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
  bool color{false};
  bool datetime{false};
  bool devutil{false};
  std::string color_hex;
  std::string color_rgb;
  std::string color_hsl;
  std::string color_hsv;
  std::string color_cmyk;
  std::string error;
};

MathResult evaluate_math(std::string_view expr);
bool convert_metric(std::string_view expr, MathResult& out);
bool convert_currency(double amount, std::string_view from, std::string_view to, MathResult& out);
bool convert_color(std::string_view expr, MathResult& out);
bool convert_datetime(std::string_view expr, MathResult& out);
bool convert_devutil(std::string_view expr, MathResult& out);
void set_currency_network_enabled(bool enabled);

}  // namespace wilfred
