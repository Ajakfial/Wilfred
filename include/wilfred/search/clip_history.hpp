#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

struct ClipEntry {
  std::string text;
  std::int64_t when{0};
  bool pinned{false};
};

// Persistent clipboard history backing the `clips` mini. Process-wide
// singleton; all methods are thread-safe. Texts are capped at 64KB each.
class ClipStore {
 public:
  static ClipStore& instance();

  // max_entries caps unpinned entries (pinned always survive). persist/path
  // control the throttled on-disk snapshot (at most one write per 10s).
  void configure(int max_entries, bool persist, std::string path);
  bool load();
  bool save_now();
  void save_throttled();

  void record(const std::string& text);
  std::vector<std::string> texts() const;  // pinned first, then most recent
  std::vector<ClipEntry> entries() const;
  bool pinned(const std::string& text) const;
  bool pin_text(const std::string& text);
  bool unpin_text(const std::string& text);
  void clear_unpinned();
  void clear_all();
  std::vector<std::string> search(const std::string& query, int limit) const;
  std::size_t size() const;

 private:
  ClipStore() = default;
  void prune_locked();
  bool save_locked() const;

  mutable std::mutex mu_;
  std::vector<ClipEntry> items_;
  int max_entries_{200};
  bool persist_{true};
  std::string path_;
  std::int64_t last_save_{0};
  bool dirty_{false};
};

}  // namespace wilfred
