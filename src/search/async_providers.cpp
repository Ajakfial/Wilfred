#include "wilfred/search/async_providers.hpp"

namespace wilfred {
namespace {

// Bounded cache so one busy index cannot grow memory without limit.
constexpr std::size_t kMaxEntries = 32;

}  // namespace

AsyncProviders::AsyncProviders(std::chrono::seconds ttl) : ttl_(ttl), worker_(&AsyncProviders::loop, this) {}

AsyncProviders::~AsyncProviders() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stop_ = true;
    slot_.has = false;
  }
  cv_.notify_all();
  if (worker_.joinable()) worker_.join();
}

void AsyncProviders::request(std::uint64_t gen, std::string query, std::size_t limit, RunFn run,
                             PushFn push) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    latest_ = gen;
    slot_.has = true;
    slot_.gen = gen;
    slot_.query = std::move(query);
    slot_.limit = limit;
    slot_.run = std::move(run);
    slot_.push = std::move(push);
  }
  cv_.notify_one();
}

bool AsyncProviders::cached(const std::string& query, std::size_t limit,
                            std::vector<SearchResult>& out) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = cache_.find(query);
  if (it == cache_.end()) return false;
  if (it->second.limit < limit) return false;
  if (std::chrono::steady_clock::now() - it->second.at >= ttl_) return false;
  out = it->second.results;
  if (out.size() > limit) out.resize(limit);
  return true;
}

void AsyncProviders::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  cache_.clear();
  slot_.has = false;
}

void AsyncProviders::loop() {
  for (;;) {
    Slot cur;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [this] { return slot_.has || stop_; });
      if (stop_) return;
      cur = std::move(slot_);
      slot_.has = false;
    }
    // Fresh cache entry: skip the subprocesses, deliver instantly unless
    // a newer generation already superseded this one.
    std::vector<SearchResult> hit;
    if (cached(cur.query, cur.limit, hit)) {
      std::uint64_t latest = 0;
      {
        std::lock_guard<std::mutex> lock(mu_);
        latest = latest_;
      }
      if (cur.gen == latest) {
        try {
          cur.push(cur.gen, std::move(hit));
        } catch (...) {
        }
      }
      continue;
    }
    std::vector<SearchResult> results;
    try {
      if (cur.run) results = cur.run();
    } catch (...) {
      continue;  // failed run pushes nothing; the fast results stand
    }
    std::uint64_t latest = 0;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (cache_.size() >= kMaxEntries) {
        auto oldest = cache_.begin();
        for (auto it = cache_.begin(); it != cache_.end(); ++it)
          if (it->second.at < oldest->second.at) oldest = it;
        cache_.erase(oldest);
      }
      Entry e;
      e.results = results;
      e.limit = cur.limit;
      e.at = std::chrono::steady_clock::now();
      cache_[cur.query] = std::move(e);
      latest = latest_;
    }
    if (cur.gen != latest) continue;  // stale: a newer query is showing
    try {
      cur.push(cur.gen, std::move(results));
    } catch (...) {
    }
  }
}

}  // namespace wilfred
