#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

// Extension point for additional search backends (cloud storage, IDEs, plugins).
class SearchProvider {
public:
  virtual ~SearchProvider() = default;
  virtual std::string id() const = 0;
  virtual std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                          std::size_t limit) = 0;
};

// Cumulative per-provider timing, surfaced in `wilfred status` (live probe)
// and the HTTP /status endpoint. Overhead is one clock read per provider.
struct ProviderStats {
  std::uint64_t calls{0};
  std::uint64_t total_us{0};
  std::uint64_t last_us{0};
  std::size_t last_hits{0};
};

class ProviderRegistry {
public:
  void add(std::unique_ptr<SearchProvider> p) { providers_.push_back(std::move(p)); }
  const std::vector<std::unique_ptr<SearchProvider>>& all() const { return providers_; }

  std::vector<SearchResult> query_all(const std::string& text, const Config& cfg,
                                      std::size_t limit) const {
    std::vector<SearchResult> out;
    for (auto& p : providers_) {
      const auto t0 = std::chrono::steady_clock::now();
      try {
        auto part = p->query(text, cfg, limit);
        record(p->id(), t0, part.size());
        out.insert(out.end(), part.begin(), part.end());
      } catch (...) {
        record(p->id(), t0, 0);
      }
    }
    return out;
  }

  std::map<std::string, ProviderStats> stats() const {
    std::lock_guard<std::mutex> lock(stats_mu_);
    return stats_;
  }
  void reset_stats() {
    std::lock_guard<std::mutex> lock(stats_mu_);
    stats_.clear();
  }

private:
  void record(const std::string& id, std::chrono::steady_clock::time_point t0,
              std::size_t hits) const {
    const auto us = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t0)
            .count());
    std::lock_guard<std::mutex> lock(stats_mu_);
    auto& s = stats_[id];
    ++s.calls;
    s.total_us += us;
    s.last_us = us;
    s.last_hits = hits;
  }

  std::vector<std::unique_ptr<SearchProvider>> providers_;
  mutable std::mutex stats_mu_;
  mutable std::map<std::string, ProviderStats> stats_;
};

}  // namespace wilfred
