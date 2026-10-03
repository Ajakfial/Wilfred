#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

#include "wilfred/core/log.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <mutex>
#include <string>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace wilfred {
#if defined(WILFRED_BSD)
namespace fs = std::filesystem;

// kqueue watcher: one EVFILT_VNODE watch per directory (kqueue watches are
// not recursive, so the tree is walked like the inotify backend does).
// A kevent wake triggers an mtime rescan+diff, which is also run every 2s
// as a safety net — same cadence and debounce contract as the other
// backends. Rename pairing is impossible with kqueue, so moves surface as
// Deleted+Created pairs; consumers already handle both shapes.
class BsdWatcher final : public WatcherBackend {
 public:
  ~BsdWatcher() override { stop(); }

  bool start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb) override {
    stop();
    cb_ = std::move(cb);
    debounce_ms_ = debounce_ms;
    kq_ = kqueue();
    if (kq_ < 0) return false;
    if (pipe(wake_) < 0) {
      close(kq_);
      kq_ = -1;
      return false;
    }
    fcntl(wake_[0], F_SETFD, FD_CLOEXEC);
    fcntl(wake_[1], F_SETFD, FD_CLOEXEC);
    {
      // Wake pipe readable event so stop() unblocks kevent immediately.
      struct kevent ev;
      EV_SET(&ev, static_cast<uintptr_t>(wake_[0]), EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0,
             nullptr);
      if (kevent(kq_, &ev, 1, nullptr, 0, nullptr) < 0) {
        close(wake_[0]);
        close(wake_[1]);
        close(kq_);
        kq_ = -1;
        wake_[0] = wake_[1] = -1;
        return false;
      }
      std::lock_guard<std::mutex> lock(mu_);
      roots_ = roots;
      snapshot_locked();
      for (auto& r : roots_) watch_tree_locked(r);
    }
    running_ = true;
    th_ = std::thread([this] { loop(); });
    return true;
  }

  void stop() override {
    running_ = false;
    if (wake_[1] >= 0) {
      char c = 1;
      auto _ = write(wake_[1], &c, 1);
    }
    if (th_.joinable()) th_.join();
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& [fd, path] : watches_) {
      (void)path;
      close(fd);
    }
    watches_.clear();
    if (kq_ >= 0) {
      close(kq_);
      kq_ = -1;
    }
    if (wake_[0] >= 0) {
      close(wake_[0]);
      wake_[0] = -1;
    }
    if (wake_[1] >= 0) {
      close(wake_[1]);
      wake_[1] = -1;
    }
  }

  void add_root(const std::string& root) override {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& r : roots_)
      if (r == root) return;
    roots_.push_back(root);
    // Pick up anything created since boot without emitting a storm:
    // snapshot first, watch second.
    scan_tree(root, snapshot_);
    watch_tree_locked(root);
  }

 private:
  void snapshot_locked() {
    snapshot_.clear();
    for (auto& r : roots_) scan_tree(r, snapshot_);
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

  // Caller holds mu_. Opens one fd per directory and registers NOTE events.
  void watch_tree_locked(const std::string& dir) {
    std::error_code ec;
    if (!fs::is_directory(fs::path(dir), ec)) return;
    int fd = open(dir.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY);
    if (fd < 0) {
      log_debug("watch", "kqueue open failed");
      return;
    }
    struct kevent ev;
    EV_SET(&ev, static_cast<uintptr_t>(fd), EVFILT_VNODE, EV_ADD | EV_CLEAR,
           NOTE_WRITE | NOTE_DELETE | NOTE_RENAME | NOTE_ATTRIB | NOTE_REVOKE, 0, nullptr);
    if (kevent(kq_, &ev, 1, nullptr, 0, nullptr) < 0) {
      log_debug("watch", "kqueue watch failed");
      close(fd);
      return;
    }
    watches_[fd] = dir;
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    std::vector<std::string> subdirs;
    while (auto* ent = readdir(d)) {
      if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
      if (ent->d_type == DT_DIR) subdirs.push_back(dir + "/" + ent->d_name);
    }
    closedir(d);
    for (auto& sub : subdirs) watch_tree_locked(sub);
  }

  // Drop watches whose paths are gone (or stopped being directories) and
  // add watches for newly created directories. Caller holds mu_.
  void reconcile_locked(const std::unordered_map<std::string, fs::file_time_type>& cur) {
    std::unordered_map<std::string, bool> is_dir;
    for (auto& [p, t] : cur) {
      (void)t;
      std::error_code ec;
      if (fs::is_directory(fs::path(p), ec)) is_dir[p] = true;
    }
    std::vector<int> dead;
    std::unordered_map<int, std::string> live;
    for (auto& [fd, path] : watches_) {
      if (is_dir.count(path))
        live[fd] = path;
      else
        dead.push_back(fd);
    }
    for (int fd : dead) {
      close(fd);
      watches_.erase(fd);
    }
    std::unordered_map<std::string, bool> watched;
    for (auto& [fd, path] : watches_) {
      (void)fd;
      watched[path] = true;
    }
    for (auto& [p, has] : is_dir) {
      (void)has;
      if (!watched.count(p)) watch_tree_locked(p);
    }
  }

  void loop() {
    struct kevent events[64];
    const struct timespec quantum{2, 0};
    while (running_) {
      int n = kevent(kq_, nullptr, 0, events, 64, &quantum);
      if (!running_) break;
      if (n < 0) continue;
      bool dirty = (n == 0);  // 2s timeout: safety-net rescan, same cadence
      for (int i = 0; i < n; ++i) {  // as the polling backends.
        if (static_cast<int>(events[i].ident) == wake_[0]) {
          char c;
          auto _ = read(wake_[0], &c, 1);
        } else {
          dirty = true;
        }
      }
      if (!dirty) continue;
      // Rescan on every wake (event or 2s timeout): emit the diff, then
      // reconcile watches with the new tree.
      std::unordered_map<std::string, fs::file_time_type> cur;
      {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& r : roots_) scan_tree(r, cur);
      }
      std::vector<FsEvent> out;
      {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& [p, t] : cur) {
          auto it = snapshot_.find(p);
          if (it == snapshot_.end())
            out.push_back(FsEvent{FsEventKind::Created, p, {}});
          else if (it->second != t)
            out.push_back(FsEvent{FsEventKind::Modified, p, {}});
        }
        for (auto& [p, t] : snapshot_) {
          (void)t;
          if (!cur.count(p)) out.push_back(FsEvent{FsEventKind::Deleted, p, {}});
        }
        snapshot_.swap(cur);
        reconcile_locked(snapshot_);
      }
      if (cb_) {
        for (auto& ev : out) {
          auto now = std::chrono::steady_clock::now();
          // Key on kind as well as path: a Modified must not suppress an
          // immediately following Deleted for the same file.
          std::string key = ev.path + "|" + std::to_string(static_cast<int>(ev.kind));
          auto& last = last_emit_[key];
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
  int kq_{-1};
  int wake_[2]{-1, -1};
  std::atomic<bool> running_{false};
  std::thread th_;
  std::mutex mu_;
  std::vector<std::string> roots_;
  std::unordered_map<int, std::string> watches_;
  std::unordered_map<std::string, fs::file_time_type> snapshot_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_emit_;
};

std::unique_ptr<WatcherBackend> create_watcher_backend() {
  return std::make_unique<BsdWatcher>();
}

#endif
}  // namespace wilfred
