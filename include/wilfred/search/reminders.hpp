#pragma once

// Scheduled reminders backing the `remind` mini. Unlike timers, reminders
// survive restarts: specs persist to a small snapshot file in the data dir
// and the service loop fires an OS notification when one comes due while
// Wilfred runs.
//
// Time syntax accepted by parse_reminder_due:
//   durations   30m, 1h30m, 90s, 1:30, 90     (bare number = minutes)
//   wall clocks 14:30, 9am, 9:30pm
//   day words   today, tonight, tomorrow      (prefix, optional "at")
//   examples    remind 30m stretch
//               remind tomorrow 9am standup prep
//               remind at 14:30 pickup kid

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

struct ReminderSpec {
  std::int64_t id{0};
  std::string label;
  std::int64_t due_ms{0};
  bool notified{false};
};

// Parses the leading time spec of `text` and reports how much of the front
// was consumed (so the remainder becomes the label). Recognizes at most one
// day-word plus one optional "at" plus either a duration or a clock time.
// Wall-clock forms that already passed today roll to tomorrow (or the named
// day). Returns false when no time spec is found at the start.
bool parse_reminder_due(const std::string& text, std::int64_t now_ms, std::int64_t& out_ms,
                        std::size_t& consumed_out);

// "in 24m", "in 2h", "due in 5m", "3h left" — relative form for card titles.
std::string format_reminder_eta(std::int64_t due_ms, std::int64_t now_ms);

// Local "2026-10-22 14:30" — absolute form for cards and the miss report.
std::string format_reminder_when(std::int64_t due_ms);

class ReminderStore {
public:
  static ReminderStore& instance();

  // Path may be empty to disable persistence (no-args default in tests).
  void configure(std::string path);
  bool load();
  bool save();

  std::string add(const std::string& label, std::int64_t due_ms);
  // Matches on id or a case-insensitive label substring ("rm 3", "rm lunch").
  std::string remove(const std::string& id_or_text);
  std::string clear_all();
  std::vector<ReminderSpec> list() const;
  std::size_t size() const;

  // All reminders due at or before `now_ms` (due first, oldest first).
  // Marks each one notified=true so it fires exactly once; entries stay in
  // the list as a "missed" record until removed or cleared.
  std::vector<ReminderSpec> due(std::int64_t now_ms);

private:
  ReminderStore() = default;
  mutable std::mutex mu_;
  std::vector<ReminderSpec> items_;
  std::int64_t next_id_{1};
  std::string path_;
};

}  // namespace wilfred
