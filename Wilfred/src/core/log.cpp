#include "wilfred/core/log.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/time_util.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace wilfred {

Logger& Logger::instance() {
  static Logger g;
  return g;
}

void Logger::set_level(LogLevel level) {
  std::lock_guard<std::mutex> lock(mu_);
  level_ = level;
}

void Logger::set_file(const std::string& path, std::uint64_t max_bytes) {
  std::lock_guard<std::mutex> lock(mu_);
  file_path_ = path;
  max_bytes_ = max_bytes ? max_bytes : 2 * 1024 * 1024;
  written_ = file_exists(path) ? file_size_bytes(path) : 0;
}

void Logger::rotate_if_needed() {
  if (file_path_.empty() || written_ < max_bytes_) return;
  std::string old = file_path_ + ".1";
  remove_file(old);
  std::rename(file_path_.c_str(), old.c_str());
  written_ = 0;
}

void Logger::log(LogLevel level, std::string_view component, std::string_view message) {
  if (static_cast<int>(level) > static_cast<int>(level_)) return;
  const char* ls = "INFO";
  if (level == LogLevel::Error) ls = "ERROR";
  else if (level == LogLevel::Warn)
    ls = "WARN";
  else if (level == LogLevel::Debug)
    ls = "DEBUG";

  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char ts[32];
  std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

  char line[2048];
  std::snprintf(line, sizeof(line), "%s [%s] %.*s: %.*s\n", ts, ls,
                static_cast<int>(component.size()), component.data(),
                static_cast<int>(message.size()), message.data());

  std::lock_guard<std::mutex> lock(mu_);
#ifdef _WIN32
  if (_isatty(_fileno(stderr)))
    std::fputs(line, stderr);
#else
  std::fputs(line, stderr);
#endif
  if (!file_path_.empty()) {
    rotate_if_needed();
    std::FILE* f = std::fopen(file_path_.c_str(), "ab");
    if (f) {
      auto n = std::strlen(line);
      std::fwrite(line, 1, n, f);
      std::fclose(f);
      written_ += n;
    }
  }
}

void log_error(std::string_view c, std::string_view m) {
  Logger::instance().log(LogLevel::Error, c, m);
}
void log_warn(std::string_view c, std::string_view m) {
  Logger::instance().log(LogLevel::Warn, c, m);
}
void log_info(std::string_view c, std::string_view m) {
  Logger::instance().log(LogLevel::Info, c, m);
}
void log_debug(std::string_view c, std::string_view m) {
  Logger::instance().log(LogLevel::Debug, c, m);
}

}  // namespace wilfred
