#pragma once

#include <cstdint>
#include <string>

namespace wilfred {

std::int64_t unix_millis();
std::int64_t unix_seconds();
std::string format_iso8601(std::int64_t unix_seconds);
std::int64_t file_time_to_unix(std::uint64_t platform_time);

}  // namespace wilfred
