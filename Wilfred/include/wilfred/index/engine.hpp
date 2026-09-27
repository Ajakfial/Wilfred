#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/index/store.hpp"
#include "wilfred/index/wal.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

struct IndexStats {
  std::uint64_t files{0};
  std::uint64_t dirs{0};
  std::uint64_t apps{0};
  std::uint64_t errors{0};
  std::uint64_t bytes{0};
  bool scanning{false};
  double last_scan_seconds{0};
};

class IndexEngine {
public:
  IndexEngine();
  ~IndexEngine();

  bool open(const std::string& dir, const Config& cfg);
  void close();
  bool upsert_file(const std::string& path);
  bool remove_path(const std::string& path);
  bool rename_path(const std::string& from, const std::string& to);
  void scan_roots(const std::vector<std::string>& extra = {});
  void compact();
  void checkpoint();

  IndexStore& store() { return store_; }
  const IndexStore& store() const { return store_; }
  IndexStats stats() const;
  const Config& config() const { return cfg_; }
  void set_config(const Config& cfg) { cfg_ = cfg; }
  std::uint64_t generation() const { return generation_.load(); }
  void bump_generation() { generation_.fetch_add(1); }

  using ProgressFn = std::function<void(const IndexStats&)>;
  void set_progress(ProgressFn fn) { progress_ = std::move(fn); }

  std::atomic<bool>& cancel() { return cancel_; }

private:
  bool persist_snapshot();
  bool recover(bool snapshot_loaded);

  Config cfg_;
  std::string dir_;
  std::string snapshot_path_;
  std::string wal_path_;
  IndexStore store_;
  WriteAheadLog wal_;
  mutable std::mutex stats_mu_;
  IndexStats stats_{};
  ProgressFn progress_;
  std::atomic<bool> cancel_{false};
  std::uint64_t dirty_{0};
  std::atomic<std::uint64_t> generation_{0};
};

}  // namespace wilfred
