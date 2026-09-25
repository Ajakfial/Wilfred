#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace wilfred {

enum class LogLevel : int { Error = 0, Warn = 1, Info = 2, Debug = 3 };

class Logger {
public:
  static Logger& instance();

  void set_level(LogLevel level);
  void set_file(const std::string& path, std::uint64_t max_bytes);
  LogLevel level() const { return level_; }

  void log(LogLevel level, std::string_view component, std::string_view message);

private:
  Logger() = default;
  void rotate_if_needed();

  std::mutex mu_;
  LogLevel level_{LogLevel::Info};
  std::string file_path_;
  std::uint64_t max_bytes_{2 * 1024 * 1024};
  std::uint64_t written_{0};
};

void log_error(std::string_view c, std::string_view m);
void log_warn(std::string_view c, std::string_view m);
void log_info(std::string_view c, std::string_view m);
void log_debug(std::string_view c, std::string_view m);

}  // namespace wilfred
