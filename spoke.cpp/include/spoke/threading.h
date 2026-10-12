#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace spoke {

inline size_t hardwareThreads() {
  unsigned n = std::thread::hardware_concurrency();
  return n == 0 ? 4 : n;
}

/// Minimal thread pool. Header-inline so tests/bench link trivially.
class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads = 0) : stop_(false) {
    if (num_threads == 0) num_threads = hardwareThreads();
    // Cap: tiny model saturates at ~8 threads; more just burns power.
    if (num_threads > 8) num_threads = 8;
    for (size_t i = 0; i < num_threads; ++i) {
      workers_.emplace_back([this] {
        for (;;) {
          std::function<void()> task;
          {
            std::unique_lock<std::mutex> lk(queue_mutex_);
            cond_.wait(lk, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
          }
          task();
        }
      });
    }
  }

  ~ThreadPool() {
    {
      std::unique_lock<std::mutex> lk(queue_mutex_);
      stop_ = true;
    }
    cond_.notify_all();
    for (auto& t : workers_) t.join();
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  size_t size() const { return workers_.size(); }

  template <typename F>
  auto enqueue(F&& f) -> std::future<decltype(f())> {
    using R = decltype(f());
    auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
    std::future<R> fut = task->get_future();
    {
      std::unique_lock<std::mutex> lk(queue_mutex_);
      tasks_.emplace([task] { (*task)(); });
    }
    cond_.notify_one();
    return fut;
  }

  void parallelFor(size_t n, const std::function<void(size_t)>& fn) {
    if (n == 0) return;
    if (workers_.empty() || n == 1) {
      for (size_t i = 0; i < n; ++i) fn(i);
      return;
    }
    size_t T = workers_.size();
    size_t chunk = (n + T - 1) / T;
    std::vector<std::future<void>> futs;
    for (size_t t = 0; t < T; ++t) {
      size_t begin = t * chunk, end = begin + chunk;
      if (begin >= n) break;
      if (end > n) end = n;
      futs.push_back(enqueue([fn, begin, end] {
        for (size_t i = begin; i < end; ++i) fn(i);
      }));
    }
    for (auto& f : futs) f.get();
  }

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex queue_mutex_;
  std::condition_variable cond_;
  bool stop_;
};

}  // namespace spoke
