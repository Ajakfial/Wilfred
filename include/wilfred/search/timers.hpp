#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

struct TimerSpec {
  std::string id;
  std::string label;
  std::int64_t total_ms{0};
  std::int64_t ends_at_ms{0};
  std::int64_t started_at_ms{0};
  bool running{false};
  bool pomodoro{false};
};

// Parse durations like "25", "25m", "10s", "1h30m", "1:30", "25:00".
// Bare numbers mean minutes. Returns false on garbage.
bool parse_duration_ms(const std::string& s, std::int64_t& out_ms);
std::string format_duration_ms(std::int64_t ms);
std::string format_remaining_ms(std::int64_t ms);

class TimerStore {
public:
  static TimerStore& instance();
  // Starts (or restarts) a timer. Returns a human message.
  std::string start(const std::string& label, std::int64_t duration_ms, bool pomodoro = false);
  std::string stop(const std::string& id_or_empty);
  void clear_finished();
  std::vector<TimerSpec> list() const;
  const TimerSpec* active() const;

private:
  TimerStore() = default;
  mutable std::mutex mu_;
  std::vector<TimerSpec> timers_;
};

// Stopwatch singleton: single running stopwatch with start/stop/reset/lap.
class Stopwatch {
public:
  static Stopwatch& instance();
  std::string start();
  std::string stop();
  std::string reset();
  std::string lap(const std::string& label = {});
  bool running() const;
  std::int64_t elapsed_ms() const;
  std::vector<std::string> laps() const;

private:
  Stopwatch() = default;
};

}  // namespace wilfred
