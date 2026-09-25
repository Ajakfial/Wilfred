#include "wilfred/core/thread_pool.hpp"

#include "wilfred/core/log.hpp"

#include <algorithm>
#include <thread>
#include <exception>

namespace wilfred {

ThreadPool::ThreadPool(std::size_t workers) {
  target_ = workers ? workers : 1;
  spawn_to(target_);
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::spawn_to(std::size_t n) {
  while (workers_.size() < n) {
    workers_.emplace_back([this] { worker_loop(); });
  }
}

void ThreadPool::set_worker_count(std::size_t n) {
  if (n == 0) n = 1;
  std::lock_guard<std::mutex> lock(mu_);
  target_ = n;
  spawn_to(n);
  cv_.notify_all();
}

void ThreadPool::post(std::function<void()> task) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (stopping_) return;
    queue_.push(std::move(task));
  }
  cv_.notify_one();
}

void ThreadPool::post_bounded(std::function<void()> task, std::size_t max_pending) {
  if (max_pending == 0) max_pending = 1;
  std::unique_lock<std::mutex> lock(mu_);
  space_cv_.wait(lock, [&] { return stopping_ || queue_.size() < max_pending; });
  if (stopping_) return;
  queue_.push(std::move(task));
  lock.unlock();
  cv_.notify_one();
}

std::size_t ThreadPool::pending() const {
  std::lock_guard<std::mutex> lock(mu_);
  return queue_.size();
}

void ThreadPool::wait_idle() {
  std::unique_lock<std::mutex> lock(mu_);
  idle_cv_.wait(lock, [&] { return queue_.empty() && active_ == 0; });
}

void ThreadPool::stop() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    stopping_ = true;
  }
  cv_.notify_all();
  for (auto& t : workers_)
    if (t.joinable()) t.join();
  workers_.clear();
}

void ThreadPool::worker_loop() {
  for (;;) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(mu_);
      cv_.wait(lock, [&] { return stopping_ || !queue_.empty(); });
      if (stopping_ && queue_.empty()) return;
      task = std::move(queue_.front());
      queue_.pop();
      ++active_;
      space_cv_.notify_all();
    }
    try {
      task();
    } catch (const std::exception& e) {
      log_error("thread_pool", e.what());
    } catch (...) {
      log_error("thread_pool", "unknown exception in worker thread");
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      --active_;
      if (queue_.empty() && active_ == 0) idle_cv_.notify_all();
    }
  }
}

std::size_t adaptive_index_workers(int cpu_percent_limit, std::size_t memory_limit_mb) {
  auto hw = std::max(1u, std::thread::hardware_concurrency());
  int pct = std::clamp(cpu_percent_limit, 10, 100);
  std::size_t by_cpu = std::max<std::size_t>(1, (hw * static_cast<unsigned>(pct)) / 100);
  std::size_t by_mem = std::max<std::size_t>(1, memory_limit_mb / 64);
  return std::max<std::size_t>(1, std::min({by_cpu, by_mem, static_cast<std::size_t>(hw)}));
}

}  // namespace wilfred
