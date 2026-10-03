#include "wilfred/platform/native.hpp"

#include "wilfred/core/log.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace wilfred {
#if defined(__ANDROID__)
namespace fs = std::filesystem;

// Polling watcher: light on battery (2s cadence, mtime-only), needs no
// inotify permissions and works inside app-private dirs and /sdcard.
class AndroidWatcher final : public WatcherBackend {
 public:
  ~AndroidWatcher() override { stop(); }

  bool start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb) override {
    stop();
    cb_ = std::move(cb);
    debounce_ms_ = debounce_ms;
    {
      std::lock_guard<std::mutex> lock(mu_);
      roots_ = roots;
      snapshot_locked();
    }
    running_ = true;
    th_ = std::thread([this] { loop(); });
    return true;
  }

  void stop() override {
    running_ = false;
    if (th_.joinable()) th_.join();
  }

  void add_root(const std::string& root) override {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& r : roots_)
      if (r == root) return;
    roots_.push_back(root);
    snapshot_locked();
  }

 private:
  void snapshot_locked() {
    mtimes_.clear();
    for (auto& r : roots_) scan_tree(r, mtimes_);
  }

  static void scan_tree(const std::string& root,
                        std::unordered_map<std::string, fs::file_time_type>& out) {
    std::error_code ec;
    if (!fs::exists(fs::path(root), ec)) return;
    for (auto it = fs::recursive_directory_iterator(fs::path(root), ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
      if (ec) break;
      std::error_code ec2;
      auto mtime = fs::last_write_time(it->path(), ec2);
      if (ec2) continue;
      out[it->path().string()] = mtime;
    }
  }

  void loop() {
    while (running_) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      if (!running_) break;
      std::unordered_map<std::string, fs::file_time_type> cur;
      std::vector<std::string> roots;
      {
        std::lock_guard<std::mutex> lock(mu_);
        roots = roots_;
      }
      for (auto& r : roots) scan_tree(r, cur);
      std::vector<FsEvent> events;
      {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [p, t] : cur) {
          auto it = mtimes_.find(p);
          if (it == mtimes_.end()) {
            events.push_back(FsEvent{FsEventKind::Created, p, {}});
          } else if (it->second != t) {
            events.push_back(FsEvent{FsEventKind::Modified, p, {}});
          }
        }
        for (auto& [p, t] : mtimes_) {
          (void)t;
          if (!cur.count(p)) events.push_back(FsEvent{FsEventKind::Deleted, p, {}});
        }
        mtimes_.swap(cur);
      }
      if (cb_) {
        for (auto& ev : events) {
          auto now = std::chrono::steady_clock::now();
          auto& last = last_emit_[ev.path];
          if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() <
              debounce_ms_)
            continue;
          last = now;
          cb_(ev);
        }
      }
    }
  }

  FsEventFn cb_;
  int debounce_ms_{80};
  std::atomic<bool> running_{false};
  std::thread th_;
  std::mutex mu_;
  std::vector<std::string> roots_;
  std::unordered_map<std::string, fs::file_time_type> mtimes_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_emit_;
};

std::unique_ptr<WatcherBackend> create_watcher_backend() {
  return std::make_unique<AndroidWatcher>();
}

#endif
}  // namespace wilfred
