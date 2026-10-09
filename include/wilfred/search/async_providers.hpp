#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace wilfred {

// Single-flight background fan-out for search providers (subprocesses and
// network: os_search, remotes, pkg, ...). The overlay shows fast index
// results immediately and merges provider hits when they land; only the
// latest requested query is ever executed, so a typing burst neither
// blocks the UI nor spawns a subprocess storm, and stale generations are
// dropped instead of displayed.
//
// Short-TTL result cache keyed by query text: repeat queries (re-summon,
// launch flows) skip the subprocesses entirely. Thread-safe; the worker
// is joined on destruction (declare after everything PushFn touches).
class AsyncProviders {
public:
  using RunFn = std::function<std::vector<SearchResult>()>;
  using PushFn = std::function<void(std::uint64_t gen, std::vector<SearchResult>)>;

  explicit AsyncProviders(std::chrono::seconds ttl = std::chrono::seconds(60));
  ~AsyncProviders();
  AsyncProviders(const AsyncProviders&) = delete;
  AsyncProviders& operator=(const AsyncProviders&) = delete;

  // Queue a provider run for `gen`. Overwrites any still-queued request so
  // only the latest query runs; an in-flight run is NOT cancelled, but its
  // push is dropped when a newer generation was requested meanwhile.
  void request(std::uint64_t gen, std::string query, std::size_t limit, RunFn run, PushFn push);

  // Instant synchronous lookup for the overlay fast path. Hit only when a
  // fresh entry covers at least `limit` results.
  bool cached(const std::string& query, std::size_t limit, std::vector<SearchResult>& out) const;

  void clear();

private:
  struct Entry {
    std::vector<SearchResult> results;
    std::size_t limit{0};
    std::chrono::steady_clock::time_point at{};
  };
  struct Slot {
    bool has{false};
    std::uint64_t gen{0};
    std::string query;
    RunFn run;
    PushFn push;
    std::size_t limit{0};
  };

  void loop();

  const std::chrono::seconds ttl_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  Slot slot_;
  std::uint64_t latest_{0};
  bool stop_{false};
  std::unordered_map<std::string, Entry> cache_;
  std::thread worker_;
};

}  // namespace wilfred
