#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace wilfred {

class ThreadPool {
public:
  explicit ThreadPool(std::size_t workers);
  ~ThreadPool();

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void post(std::function<void()> task);
  void post_bounded(std::function<void()> task, std::size_t max_pending);
  void set_worker_count(std::size_t n);
  std::size_t worker_count() const { return workers_.size(); }
  std::size_t pending() const;
  void wait_idle();
  void stop();

private:
  void worker_loop();
  void spawn_to(std::size_t n);

  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::condition_variable idle_cv_;
  std::condition_variable space_cv_;
  std::queue<std::function<void()>> queue_;
  std::vector<std::thread> workers_;
  std::atomic<bool> stopping_{false};
  std::size_t active_{0};
  std::size_t target_{0};
};

std::size_t adaptive_index_workers(int cpu_percent_limit, std::size_t memory_limit_mb);

}  // namespace wilfred
