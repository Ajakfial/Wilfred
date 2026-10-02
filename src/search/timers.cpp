#include "wilfred/search/timers.hpp"

#include "wilfred/core/time_util.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <mutex>

namespace wilfred {

bool parse_duration_ms(const std::string& s, std::int64_t& out_ms) {
  std::string t;
  for (char c : s) {
    if (c != ' ' && c != '\t') t.push_back(c);
  }
  if (t.empty()) return false;
  for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  // mm:ss or hh:mm:ss
  auto colon = t.find(':');
  if (colon != std::string::npos) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : t + ":") {
      if (c == ':') {
        parts.push_back(cur);
        cur.clear();
      } else {
        cur.push_back(c);
      }
    }
    if (parts.size() < 2 || parts.size() > 3) return false;
    std::int64_t h = 0, m = 0, sec = 0;
    try {
      if (parts.size() == 2) {
        m = std::stoll(parts[0]);
        sec = std::stoll(parts[1]);
      } else {
        h = std::stoll(parts[0]);
        m = std::stoll(parts[1]);
        sec = std::stoll(parts[2]);
      }
    } catch (...) {
      return false;
    }
    if (m < 0 || m >= 10000 || sec < 0 || sec >= 60 || h < 0 || h > 1000) return false;
    out_ms = ((h * 3600) + (m * 60) + sec) * 1000;
    return out_ms > 0 && out_ms <= 1000LL * 3600 * 24 * 2;
  }
  // Suffix form: 1h30m, 25m, 10s, 90.
  std::int64_t total = 0;
  std::string num;
  auto flush = [&](char unit) -> bool {
    if (num.empty()) return false;
    std::int64_t v = 0;
    try {
      v = std::stoll(num);
    } catch (...) {
      return false;
    }
    if (v < 0 || v > 100000) return false;
    if (unit == 'h')
      total += v * 3600 * 1000;
    else if (unit == 'm')
      total += v * 60 * 1000;
    else if (unit == 's')
      total += v * 1000;
    else
      return false;
    num.clear();
    return true;
  };
  for (std::size_t i = 0; i < t.size(); ++i) {
    char c = t[i];
    if (c >= '0' && c <= '9') {
      num.push_back(c);
    } else if (c == 'h' || c == 'm' || c == 's') {
      if (!flush(c)) return false;
    } else {
      return false;
    }
  }
  if (!num.empty()) {
    // Bare number means minutes.
    std::int64_t v = 0;
    try {
      v = std::stoll(num);
    } catch (...) {
      return false;
    }
    if (v <= 0 || v > 100000) return false;
    total += v * 60 * 1000;
  }
  if (total <= 0 || total > 1000LL * 3600 * 24 * 2) return false;
  out_ms = total;
  return true;
}

std::string format_duration_ms(std::int64_t ms) {
  if (ms < 0) ms = 0;
  auto s = ms / 1000;
  auto h = s / 3600;
  auto m = (s % 3600) / 60;
  auto sec = s % 60;
  char buf[64];
  if (h)
    std::snprintf(buf, sizeof(buf), "%lldh %02lldm %02llds", (long long)h, (long long)m,
                  (long long)sec);
  else if (m)
    std::snprintf(buf, sizeof(buf), "%lldm %02llds", (long long)m, (long long)sec);
  else
    std::snprintf(buf, sizeof(buf), "%llds", (long long)sec);
  return buf;
}

std::string format_remaining_ms(std::int64_t ms) {
  if (ms <= 0) return "done";
  return format_duration_ms(ms) + " left";
}

TimerStore& TimerStore::instance() {
  static TimerStore inst;
  return inst;
}

std::string TimerStore::start(const std::string& label, std::int64_t duration_ms, bool pomodoro) {
  std::lock_guard<std::mutex> lock(mu_);
  timers_.erase(std::remove_if(timers_.begin(), timers_.end(),
                               [](const TimerSpec& t) {
                                 return !t.running;
                               }),
                timers_.end());
  TimerSpec t;
  t.id = label.empty() ? "timer" : label;
  t.label = label.empty() ? (pomodoro ? "pomodoro" : "timer") : label;
  t.total_ms = duration_ms;
  t.started_at_ms = unix_millis();
  t.ends_at_ms = t.started_at_ms + duration_ms;
  t.running = true;
  t.pomodoro = pomodoro;
  timers_.push_back(t);
  return t.label + " started · " + format_duration_ms(duration_ms);
}

std::string TimerStore::stop(const std::string& id_or_empty) {
  std::lock_guard<std::mutex> lock(mu_);
  if (timers_.empty()) return "No timer running";
  if (id_or_empty.empty()) {
    timers_.clear();
    return "Timer stopped";
  }
  std::string want = id_or_empty;
  for (char& c : want) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  for (auto it = timers_.begin(); it != timers_.end(); ++it) {
    std::string l = it->label;
    for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (l.find(want) != std::string::npos) {
      std::string msg = it->label + " stopped";
      timers_.erase(it);
      return msg;
    }
  }
  return "No timer matching \"" + id_or_empty + "\"";
}

void TimerStore::clear_finished() {
  std::lock_guard<std::mutex> lock(mu_);
  auto now = unix_millis();
  timers_.erase(std::remove_if(timers_.begin(), timers_.end(),
                               [now](const TimerSpec& t) {
                                 return now >= t.ends_at_ms;
                               }),
                timers_.end());
}

std::vector<TimerSpec> TimerStore::list() const {
  std::lock_guard<std::mutex> lock(mu_);
  return timers_;
}

const TimerSpec* TimerStore::active() const {
  std::lock_guard<std::mutex> lock(mu_);
  if (timers_.empty()) return nullptr;
  return &timers_.front();
}

// --- Stopwatch ---

namespace {
std::mutex g_sw_mu;
bool g_sw_running = false;
std::int64_t g_sw_start = 0;
std::int64_t g_sw_accum = 0;
std::vector<std::string> g_sw_laps;
}  // namespace

Stopwatch& Stopwatch::instance() {
  static Stopwatch inst;
  return inst;
}

std::string Stopwatch::start() {
  std::lock_guard<std::mutex> lock(g_sw_mu);
  if (g_sw_running) return "Stopwatch already running · " + format_duration_ms(elapsed_ms());
  g_sw_running = true;
  g_sw_start = unix_millis();
  return "Stopwatch started";
}

std::string Stopwatch::stop() {
  std::lock_guard<std::mutex> lock(g_sw_mu);
  if (!g_sw_running) return "Stopwatch is not running";
  g_sw_accum += unix_millis() - g_sw_start;
  g_sw_running = false;
  return "Stopwatch stopped · " + format_duration_ms(g_sw_accum);
}

std::string Stopwatch::reset() {
  std::lock_guard<std::mutex> lock(g_sw_mu);
  g_sw_running = false;
  g_sw_accum = 0;
  g_sw_laps.clear();
  return "Stopwatch reset";
}

std::string Stopwatch::lap(const std::string& label) {
  std::lock_guard<std::mutex> lock(g_sw_mu);
  auto el = g_sw_accum + (g_sw_running ? (unix_millis() - g_sw_start) : 0);
  char buf[128];
  std::snprintf(buf, sizeof(buf), "Lap %zu · %s%s%s", g_sw_laps.size() + 1,
                format_duration_ms(el).c_str(), label.empty() ? "" : " · ",
                label.c_str());
  g_sw_laps.emplace_back(buf);
  return g_sw_laps.back();
}

bool Stopwatch::running() const {
  std::lock_guard<std::mutex> lock(g_sw_mu);
  return g_sw_running;
}

std::int64_t Stopwatch::elapsed_ms() const {
  std::lock_guard<std::mutex> lock(g_sw_mu);
  if (!g_sw_running) return g_sw_accum;
  return g_sw_accum + (unix_millis() - g_sw_start);
}

std::vector<std::string> Stopwatch::laps() const {
  std::lock_guard<std::mutex> lock(g_sw_mu);
  return g_sw_laps;
}

}  // namespace wilfred
