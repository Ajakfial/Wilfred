#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace wilfred {

struct YamlValue;

using YamlMap = std::map<std::string, YamlValue>;
using YamlList = std::vector<YamlValue>;

struct YamlValue {
  std::variant<std::monostate, std::string, YamlList, YamlMap> data;

  bool is_null() const { return std::holds_alternative<std::monostate>(data); }
  bool is_string() const { return std::holds_alternative<std::string>(data); }
  bool is_list() const { return std::holds_alternative<YamlList>(data); }
  bool is_map() const { return std::holds_alternative<YamlMap>(data); }

  const std::string& as_string() const { return std::get<std::string>(data); }
  const YamlList& as_list() const { return std::get<YamlList>(data); }
  const YamlMap& as_map() const { return std::get<YamlMap>(data); }
  YamlMap& as_map() { return std::get<YamlMap>(data); }

  const YamlValue* get(std::string_view key) const;
  std::string str(std::string_view key, std::string_view def = "") const;
  bool boolean(std::string_view key, bool def = false) const;
  std::int64_t integer(std::string_view key, std::int64_t def = 0) const;
  double number(std::string_view key, double def = 0) const;
  std::vector<std::string> string_list(std::string_view key) const;
};

struct YamlError {
  int line{0};
  int column{0};
  std::string message;
};

bool parse_yaml(std::string_view text, YamlValue& out, YamlError& err);
std::string yaml_to_string(const YamlValue& v);

}  // namespace wilfred
