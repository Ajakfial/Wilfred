#include "wilfred/core/time_util.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {

std::int64_t unix_millis() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::int64_t unix_seconds() { return unix_millis() / 1000; }

std::string format_iso8601(std::int64_t secs) {
  std::time_t t = static_cast<std::time_t>(secs);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &t);
#else
  gmtime_r(&t, &tm);
#endif
  std::ostringstream os;
  os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return os.str();
}

std::int64_t file_time_to_unix(std::uint64_t platform_time) {
#ifdef _WIN32
  // Windows FILETIME is 100-ns since 1601.
  constexpr std::uint64_t EPOCH_DIFF = 116444736000000000ull;
  if (platform_time < EPOCH_DIFF) return 0;
  return static_cast<std::int64_t>((platform_time - EPOCH_DIFF) / 10000000ull);
#else
  return static_cast<std::int64_t>(platform_time);
#endif
}

}  // namespace wilfred
