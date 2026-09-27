#include "wilfred/platform/native.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#ifndef _WIN32
#ifndef __APPLE__
#include <dirent.h>
#include <fcntl.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <poll.h>
#endif
#endif

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)

class LinuxWatcher final : public WatcherBackend {
public:
  ~LinuxWatcher() override { stop(); }

  bool start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb) override {
    cb_ = std::move(cb);
    debounce_ms_ = debounce_ms;
    fd_ = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (fd_ < 0) return false;
    if (pipe2(wakeup_fd_, O_CLOEXEC | O_NONBLOCK) < 0) {
      close(fd_);
      return false;
    }
    running_ = true;
    for (auto& r : roots) add_root(r);
    th_ = std::thread([this] { loop(); });
    return true;
  }

  void stop() override {
    running_ = false;
    if (wakeup_fd_[1] >= 0) {
      char c = 1;
      auto _ = write(wakeup_fd_[1], &c, 1);
    }
    if (th_.joinable()) th_.join();
    if (fd_ >= 0) {
      close(fd_);
      fd_ = -1;
    }
    if (wakeup_fd_[0] >= 0) { close(wakeup_fd_[0]); wakeup_fd_[0] = -1; }
    if (wakeup_fd_[1] >= 0) { close(wakeup_fd_[1]); wakeup_fd_[1] = -1; }
    wd_.clear();
  }

  void add_root(const std::string& root) override {
    watch_tree(root);
  }

private:
  void watch_tree(const std::string& dir) {
    int wd = inotify_add_watch(fd_, dir.c_str(),
                               IN_CREATE | IN_DELETE | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO |
                                   IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF);
    if (wd < 0) {
      log_debug("watch", "inotify add failed");
      return;
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      wd_[wd] = dir;
    }
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (auto* ent = readdir(d)) {
      if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0) continue;
      std::string p = dir + "/" + ent->d_name;
      if (ent->d_type == DT_DIR) watch_tree(p);
    }
    closedir(d);
  }

  void loop() {
    alignas(inotify_event) char buf[4096];
    struct pollfd pfd[2];
    pfd[0].fd = fd_;
    pfd[0].events = POLLIN;
    pfd[1].fd = wakeup_fd_[0];
    pfd[1].events = POLLIN;
    
    while (running_) {
      int ret = poll(pfd, 2, -1);
      if (ret < 0) continue;
      
      if (pfd[1].revents & POLLIN) {
        char c;
        auto _ = read(wakeup_fd_[0], &c, 1);
      }
      
      if (!(pfd[0].revents & POLLIN)) continue;
      
      auto n = read(fd_, buf, sizeof(buf));
      if (n <= 0) continue;
      std::size_t i = 0;
      while (i < static_cast<std::size_t>(n)) {
        auto* ev = reinterpret_cast<inotify_event*>(buf + i);
        std::string dir;
        {
          std::lock_guard<std::mutex> lock(mu_);
          auto it = wd_.find(ev->wd);
          dir = it == wd_.end() ? std::string() : it->second;
        }
        std::string name = ev->len ? std::string(ev->name) : std::string();
        std::string path = name.empty() ? dir : dir + "/" + name;
        FsEvent fe;
        fe.path = path;
        if (ev->mask & IN_CREATE) {
          fe.kind = FsEventKind::Created;
          if (ev->mask & IN_ISDIR) watch_tree(path);
        } else if (ev->mask & IN_DELETE)
          fe.kind = FsEventKind::Deleted;
        else if (ev->mask & IN_MOVED_FROM) {
          cookie_[ev->cookie] = path;
          fe.kind = FsEventKind::Renamed;
        } else if (ev->mask & IN_MOVED_TO) {
          fe.kind = FsEventKind::Renamed;
          auto c = cookie_.find(ev->cookie);
          if (c != cookie_.end()) {
            fe.path = c->second;
            fe.new_path = path;
            cookie_.erase(c);
          } else
            fe.kind = FsEventKind::Created;
        } else
          fe.kind = FsEventKind::Modified;
        if (cb_ && !(ev->mask & IN_MOVED_FROM)) debounce(fe);
        i += sizeof(inotify_event) + ev->len;
      }
    }
  }

  void debounce(const FsEvent& ev) {
    auto now = std::chrono::steady_clock::now();
    auto& t = last_[ev.path];
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - t).count() < debounce_ms_)
      return;
    t = now;
    cb_(ev);
  }

  FsEventFn cb_;
  int debounce_ms_{80};
  int fd_{-1};
  int wakeup_fd_[2]{-1, -1};
  std::atomic<bool> running_{false};
  std::thread th_;
  std::mutex mu_;
  std::unordered_map<int, std::string> wd_;
  std::unordered_map<std::uint32_t, std::string> cookie_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_;
};

std::unique_ptr<WatcherBackend> create_watcher_backend() {
  return std::make_unique<LinuxWatcher>();
}

#endif
}  // namespace wilfred
