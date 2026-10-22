#include "wilfred/search/reminders.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/search/timers.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace wilfred {

namespace {

constexpr std::size_t kMaxLabel = 512;
constexpr std::size_t kMaxItems = 512;
const char kMagic[8] = {'W', 'R', 'M', 'D', '0', '1', '\n', '\n'};

void put_u32(std::string& o, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    o.push_back(static_cast<char>((v >> (i * 8)) & 0xff));
}

void put_i64(std::string& o, std::int64_t v) {
  for (int i = 0; i < 8; ++i)
    o.push_back(static_cast<char>((static_cast<std::uint64_t>(v) >> (i * 8)) & 0xff));
}

bool take_u32(const char*& p, const char* end, std::uint32_t& v) {
  if (end - p < 4) return false;
  v = static_cast<std::uint8_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
      (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
  p += 4;
  return true;
}

bool take_i64(const char*& p, const char* end, std::int64_t& v) {
  if (end - p < 8) return false;
  std::uint64_t u = 0;
  for (int i = 0; i < 8; ++i)
    u |= static_cast<std::uint64_t>(static_cast<std::uint8_t>(p[i])) << (i * 8);
  p += 8;
  v = static_cast<std::int64_t>(u);
  return true;
}

std::string lower_copy(const std::string& s) { return to_lower_utf8(s); }

// "9am", "9:30pm", "14:30", "09:05" -> hour/min in 24h form. Bare generic
// numbers are durations, not clocks, so hour-only needs an am/pm suffix.
bool parse_clock_token(const std::string& tok, int& out_h, int& out_m) {
  std::string t = lower_copy(tok);
  bool pm = false;
  bool am = false;
  auto strip_ampm = [&](const char* suf) {
    std::size_t n = std::strlen(suf);
    if (t.size() > n && t.compare(t.size() - n, n, suf) == 0) {
      t.resize(t.size() - n);
      if (suf[0] == 'p')
        pm = true;
      else
        am = true;
      return true;
    }
    return false;
  };
  strip_ampm("pm");
  strip_ampm("am");
  auto sep = t.find(':');
  int h = 0;
  int m = 0;
  try {
    if (sep != std::string::npos) {
      if (sep == 0 || sep + 1 >= t.size()) return false;
      h = std::stoi(t.substr(0, sep));
      m = std::stoi(t.substr(sep + 1));
    } else {
      if (t.empty()) return false;
      for (char c : t)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
      h = std::stoi(t);
      m = 0;
    }
  } catch (...) {
    return false;
  }
  if (!sep && !am && !pm) return false;  // bare number = duration, not clock
  if (am || pm) {
    if (h < 1 || h > 12 || m < 0 || m > 59) return false;
    if (am && h == 12) h = 0;
    if (pm && h == 12) h = 12;
    if (pm && h < 12) h += 12;
  } else {
    if (h < 0 || h > 23 || m < 0 || m > 59) return false;
  }
  out_h = h;
  out_m = m;
  return true;
}

bool parse_dayword(const std::string& tok, int& day_offset) {
  if (tok == "today" || tok == "tonight") {
    day_offset = 0;
    return true;
  }
  if (tok == "tomorrow") {
    day_offset = 1;
    return true;
  }
  return false;
}

std::int64_t local_midnight_ms(std::int64_t now_ms) {
  std::time_t t = static_cast<std::time_t>(now_ms / 1000);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;
  tm.tm_isdst = -1;
  std::time_t mid = std::mktime(&tm);
  if (mid == static_cast<std::time_t>(-1)) {
    // Fallback: UTC-epoch day boundary. Unusual but keeps arithmetic sane.
    return (now_ms / (24 * 3600 * 1000)) * 24 * 3600 * 1000;
  }
  return static_cast<std::int64_t>(mid) * 1000;
}

}  // namespace

bool parse_reminder_due(const std::string& text, std::int64_t now_ms, std::int64_t& out_ms,
                        std::size_t& consumed_out) {
  consumed_out = 0;
  // Greedy leading tokens: one optional day word, one optional "at", then
  // either a duration ("30m", "1h30m") or a wall-clock ("14:30", "9:30pm").
  // Everything after the first non-time token is the label.
  std::size_t pos = 0;
  std::size_t end = 0;
  int day_offset = -1;
  bool at_seen = false;
  std::int64_t dur_ms = 0;
  int clock_h = -1;
  int clock_m = -1;
  while (pos < text.size()) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) ++pos;
    std::size_t start = pos;
    while (pos < text.size() && text[pos] != ' ' && text[pos] != '\t') ++pos;
    if (start == pos) break;
    auto tok = lower_copy(text.substr(start, pos - start));
    int day = 0;
    if (day_offset < 0 && parse_dayword(tok, day)) {
      day_offset = day;
      end = pos;
      continue;
    }
    if (!at_seen && tok == "at") {
      at_seen = true;
      end = pos;
      continue;
    }
    std::int64_t d = 0;
    if (dur_ms == 0 && parse_duration_ms(tok, d)) {
      dur_ms = d;
      end = pos;
      continue;
    }
    int h = 0;
    int m = 0;
    if (clock_h < 0 && parse_clock_token(tok, h, m)) {
      clock_h = h;
      clock_m = m;
      end = pos;
      continue;
    }
    break;
  }
  bool have_dur = dur_ms != 0;
  bool have_clock = clock_h >= 0;
  if (!have_dur && !have_clock) return false;
  if (have_clock) {
    std::int64_t base = local_midnight_ms(now_ms);
    if (day_offset > 0) base += static_cast<std::int64_t>(day_offset) * 24 * 3600 * 1000;
    out_ms = base + clock_h * 3600 * 1000 + clock_m * 60 * 1000;
    // Today's slot already passed: roll to tomorrow unless a later day is
    // named explicitly (tomorrow can never be in the past by definition).
    if (out_ms <= now_ms && day_offset < 1) out_ms += 24 * 3600 * 1000;
  } else {
    out_ms = now_ms + dur_ms;
  }
  if (end == 0) return false;  // safety: must have consumed at least one token
  consumed_out = end;
  return true;
}

std::string format_reminder_eta(std::int64_t due_ms, std::int64_t now_ms) {
  auto diff = due_ms - now_ms;
  if (diff <= 0) return "due now";
  return "in " + format_duration_ms(diff);
}

std::string format_reminder_when(std::int64_t due_ms) {
  std::time_t t = static_cast<std::time_t>(due_ms / 1000);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[64]{};
  std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", &tm);
  return buf;
}

ReminderStore& ReminderStore::instance() {
  static ReminderStore inst;
  return inst;
}

void ReminderStore::configure(std::string path) {
  std::lock_guard<std::mutex> lock(mu_);
  path_ = std::move(path);
}

bool ReminderStore::save() {
  std::string path;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
  }
  if (path.empty()) return false;
  std::string blob(kMagic, 8);
  std::vector<ReminderSpec> items;
  {
    std::lock_guard<std::mutex> lock(mu_);
    items = items_;
  }
  put_u32(blob, static_cast<std::uint32_t>(items.size()));
  for (auto& e : items) {
    put_i64(blob, e.id);
    put_i64(blob, e.due_ms);
    blob.push_back(e.notified ? 1 : 0);
    auto n = std::min<std::size_t>(e.label.size(), kMaxLabel);
    put_u32(blob, static_cast<std::uint32_t>(n));
    blob.append(e.label.data(), n);
  }
  create_directories(path_parent(path));
  return write_file_atomic(path, blob.data(), blob.size());
}

bool ReminderStore::load() {
  std::string path;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
  }
  if (path.empty()) return false;
  std::string blob;
  if (!read_file_all(path, blob) || blob.size() < 12) return false;
  if (std::memcmp(blob.data(), kMagic, 8) != 0) {
    log_warn("reminders", "unrecognized reminder store format");
    return false;
  }
  const char* p = blob.data() + 8;
  const char* end = blob.data() + blob.size();
  std::uint32_t count = 0;
  if (!take_u32(p, end, count)) return false;
  if (count > kMaxItems * 4) return false;
  std::vector<ReminderSpec> items;
  for (std::uint32_t i = 0; i < count; ++i) {
    std::int64_t id = 0;
    std::int64_t due = 0;
    std::uint32_t len = 0;
    if (!take_i64(p, end, id) || !take_i64(p, end, due) || p >= end) return false;
    bool notified = *p++ != 0;
    if (!take_u32(p, end, len)) return false;
    if (len > kMaxLabel || end - p < static_cast<std::ptrdiff_t>(len)) return false;
    ReminderSpec e;
    e.id = id;
    e.due_ms = due;
    e.notified = notified;
    e.label.assign(p, len);
    p += len;
    if (id > 0) items.push_back(std::move(e));
  }
  std::lock_guard<std::mutex> lock(mu_);
  items_ = std::move(items);
  for (auto& e : items_) next_id_ = std::max(next_id_, e.id + 1);
  // Truncate to the cap, oldest due first.
  std::stable_sort(items_.begin(), items_.end(),
                   [](const ReminderSpec& a, const ReminderSpec& b) {
                     if (a.notified != b.notified) return !a.notified;
                     return a.due_ms < b.due_ms;
                   });
  if (items_.size() > kMaxItems) items_.resize(kMaxItems);
  return true;
}

std::string ReminderStore::add(const std::string& label, std::int64_t due_ms) {
  ReminderSpec e;
  {
    std::lock_guard<std::mutex> lock(mu_);
    e.id = next_id_++;
    e.label = label.size() > kMaxLabel ? label.substr(0, kMaxLabel) : label;
    e.due_ms = due_ms;
    if (items_.size() >= kMaxItems) {
      // Drop the oldest already-notified entry first, else the furthest out.
      auto slot = items_.begin();
      for (auto it = items_.begin(); it != items_.end(); ++it) {
        if (it->notified) {
          slot = it;
          break;
        }
        if (it->due_ms > slot->due_ms) slot = it;
      }
      items_.erase(slot);
    }
    items_.push_back(e);
  }
  save();
  return "Reminder #" + std::to_string(e.id) + " · " + e.label;
}

std::string ReminderStore::remove(const std::string& id_or_text) {
  bool removed = false;
  std::string msg;
  {
    std::lock_guard<std::mutex> lock(mu_);
    std::string want = lower_copy(id_or_text);
    // Exact id first ("remind rm 3"), then case-insensitive label substring.
    for (auto it = items_.begin(); it != items_.end(); ++it) {
      if (want == std::to_string(it->id)) {
        msg = "Removed reminder #" + want + " · " + it->label;
        items_.erase(it);
        removed = true;
        break;
      }
    }
    if (!removed && !want.empty()) {
      for (auto it = items_.begin(); it != items_.end(); ++it) {
        if (lower_copy(it->label).find(want) != std::string::npos) {
          msg = "Removed reminder #" + std::to_string(it->id) + " · " + it->label;
          items_.erase(it);
          removed = true;
          break;
        }
      }
    }
  }
  if (removed) save();
  return removed ? msg : "No reminder matching \"" + id_or_text + "\"";
}

std::string ReminderStore::clear_all() {
  bool had = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    had = !items_.empty();
    items_.clear();
  }
  if (had) save();
  return had ? std::string("All reminders cleared") : std::string("No reminders to clear");
}

std::vector<ReminderSpec> ReminderStore::list() const {
  std::lock_guard<std::mutex> lock(mu_);
  auto items = items_;
  // Due (unnotified, soonest) first; already-rang entries trail.
  std::stable_sort(items.begin(), items.end(),
                   [](const ReminderSpec& a, const ReminderSpec& b) {
                     if (a.notified != b.notified) return !a.notified;
                     return a.due_ms < b.due_ms;
                   });
  return items;
}

std::size_t ReminderStore::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return items_.size();
}

std::vector<ReminderSpec> ReminderStore::due(std::int64_t now_ms) {
  std::vector<ReminderSpec> fired;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : items_) {
      if (!e.notified && e.due_ms <= now_ms) {
        e.notified = true;
        fired.push_back(e);
      }
    }
  }
  if (!fired.empty()) save();
  return fired;
}

}  // namespace wilfred
