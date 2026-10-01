#include "wilfred/search/clip_history.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace wilfred {

namespace {

constexpr std::size_t kMaxText = 65536;
constexpr std::int64_t kSaveIntervalSec = 10;
const char kMagic[8] = {'W', 'C', 'L', 'P', '0', '1', '\n', '\n'};

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

}  // namespace

ClipStore& ClipStore::instance() {
  static ClipStore inst;
  return inst;
}

void ClipStore::configure(int max_entries, bool persist, std::string path) {
  std::lock_guard<std::mutex> lock(mu_);
  max_entries_ = max_entries < 0 ? 0 : max_entries;
  persist_ = persist;
  path_ = std::move(path);
  prune_locked();
}

bool ClipStore::load() {
  std::string path;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
    if (!persist_ || path.empty()) return false;
  }
  std::string blob;
  if (!read_file_all(path, blob) || blob.size() < 16) return false;
  if (std::memcmp(blob.data(), kMagic, 8) != 0) {
    log_warn("clips", "unrecognized clipboard history format");
    return false;
  }
  const char* p = blob.data() + 8;
  const char* end = blob.data() + blob.size();
  std::uint32_t count = 0;
  if (!take_u32(p, end, count)) return false;
  if (count > 100000) return false;
  std::vector<ClipEntry> items;
  for (std::uint32_t i = 0; i < count; ++i) {
    if (p >= end) return false;
    bool pinned = *p++ != 0;
    std::int64_t when = 0;
    std::uint32_t len = 0;
    if (!take_i64(p, end, when) || !take_u32(p, end, len)) return false;
    if (len > 4 * 1024 * 1024 || end - p < static_cast<std::ptrdiff_t>(len)) return false;
    ClipEntry e;
    e.pinned = pinned;
    e.when = when;
    e.text.assign(p, len);
    p += len;
    if (!e.text.empty()) items.push_back(std::move(e));
  }
  std::lock_guard<std::mutex> lock(mu_);
  items_ = std::move(items);
  prune_locked();
  dirty_ = false;
  return true;
}

bool ClipStore::save_locked() const {
  if (!persist_ || path_.empty()) return false;
  std::string blob(kMagic, 8);
  put_u32(blob, static_cast<std::uint32_t>(items_.size()));
  for (auto& e : items_) {
    blob.push_back(e.pinned ? 1 : 0);
    put_i64(blob, e.when);
    auto n = std::min<std::size_t>(e.text.size(), kMaxText);
    put_u32(blob, static_cast<std::uint32_t>(n));
    blob.append(e.text.data(), n);
  }
  create_directories(path_parent(path_));
  return write_file_atomic(path_, blob.data(), blob.size());
}

bool ClipStore::save_now() {
  std::lock_guard<std::mutex> lock(mu_);
  last_save_ = unix_seconds();
  dirty_ = false;
  return save_locked();
}

void ClipStore::save_throttled() {
  std::lock_guard<std::mutex> lock(mu_);
  if (!persist_ || path_.empty()) return;
  auto now = unix_seconds();
  if (now - last_save_ < kSaveIntervalSec) {
    dirty_ = true;
    return;
  }
  last_save_ = now;
  dirty_ = false;
  if (!save_locked()) dirty_ = true;
}

void ClipStore::prune_locked() {
  // Pinned entries always survive; drop oldest unpinned beyond the cap.
  std::size_t unpinned = 0;
  for (auto& e : items_)
    if (!e.pinned) ++unpinned;
  // items_ is most-recent-first, so drop from the back.
  for (auto it = items_.end(); it != items_.begin() && unpinned > static_cast<std::size_t>(max_entries_);) {
    --it;
    if (!it->pinned) {
      it = items_.erase(it);
      --unpinned;
    }
  }
}

void ClipStore::record(const std::string& text) {
  if (text.empty()) return;
  auto clipped = text.size() > kMaxText ? text.substr(0, kMaxText) : text;
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    bool was_pinned = false;
    for (auto it = items_.begin(); it != items_.end(); ++it) {
      if (it->text == clipped) {
        was_pinned = it->pinned;
        items_.erase(it);
        changed = true;
        break;
      }
    }
    ClipEntry e;
    e.text = clipped;
    e.when = unix_seconds();
    e.pinned = was_pinned;
    items_.insert(items_.begin(), std::move(e));
    prune_locked();
    changed = true;
  }
  if (changed) save_throttled();
}

std::vector<std::string> ClipStore::texts() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> out;
  out.reserve(items_.size());
  for (auto& e : items_)
    if (e.pinned) out.push_back(e.text);
  for (auto& e : items_)
    if (!e.pinned) out.push_back(e.text);
  return out;
}

std::vector<ClipEntry> ClipStore::entries() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<ClipEntry> out;
  for (auto& e : items_)
    if (e.pinned) out.push_back(e);
  for (auto& e : items_)
    if (!e.pinned) out.push_back(e);
  return out;
}

bool ClipStore::pinned(const std::string& text) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& e : items_)
    if (e.text == text) return e.pinned;
  return false;
}

bool ClipStore::pin_text(const std::string& text) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : items_) {
      if (e.text == text && !e.pinned) {
        e.pinned = true;
        changed = true;
        break;
      }
    }
  }
  if (changed) save_throttled();
  return changed;
}

bool ClipStore::unpin_text(const std::string& text) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : items_) {
      if (e.text == text && e.pinned) {
        e.pinned = false;
        changed = true;
        break;
      }
    }
    if (changed) prune_locked();
  }
  if (changed) save_throttled();
  return changed;
}

void ClipStore::clear_unpinned() {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto n0 = items_.size();
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [](const ClipEntry& e) { return !e.pinned; }),
                 items_.end());
    changed = items_.size() != n0;
  }
  if (changed) save_throttled();
}

void ClipStore::clear_all() {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    changed = !items_.empty();
    items_.clear();
  }
  if (changed) save_throttled();
}

std::vector<std::string> ClipStore::search(const std::string& query, int limit) const {
  auto q = fold_search(normalize_query(query));
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::string> out;
  auto consider = [&](const ClipEntry& e) {
    if (limit > 0 && static_cast<int>(out.size()) >= limit) return;
    if (q.empty() || fold_search(e.text).find(q) != std::string::npos) out.push_back(e.text);
  };
  for (auto& e : items_)
    if (e.pinned) consider(e);
  for (auto& e : items_)
    if (!e.pinned) consider(e);
  return out;
}

std::size_t ClipStore::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return items_.size();
}

}  // namespace wilfred
