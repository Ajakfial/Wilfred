#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

std::string json_escape(const std::string& s);
std::string json_get_string(const std::string& s, const char* key);
int json_get_int(const std::string& s, const char* key, int def = 0);
bool json_get_bool(const std::string& s, const char* key, bool def = false);
std::string json_extract_array(const std::string& s, const char* key);
std::vector<std::string> json_object_array(const std::string& array_json);

}  // namespace wilfred
