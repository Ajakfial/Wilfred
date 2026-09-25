#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

std::string to_lower_utf8(std::string_view s);
std::string normalize_query(std::string_view s);
bool starts_with_ci(std::string_view hay, std::string_view needle);
bool contains_ci(std::string_view hay, std::string_view needle);

#ifdef _WIN32
std::wstring utf8_to_wide(std::string_view s);
std::string wide_to_utf8(std::wstring_view s);
#endif

std::vector<std::uint32_t> utf8_codepoints(std::string_view s);
std::string fold_ascii(std::string_view s);

}  // namespace wilfred
