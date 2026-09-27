#include "wilfred/platform/native.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include <deque>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {

#ifdef _WIN32

struct WatchTarget {
  std::string root;
  HANDLE dir{INVALID_HANDLE_VALUE};
  OVERLAPPED ov{};
  std::vector<char> buf;
};

class WinWatcher final : public WatcherBackend {
public:
  ~WinWatcher() override { stop(); }

  bool start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb) override {
    cb_ = std::move(cb);
    debounce_ms_ = debounce_ms;
    running_ = true;
    if (!iocp_) iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    for (auto& r : roots) add_root(r);
    th_ = std::thread([this] { loop(); });
    return iocp_ != nullptr && iocp_ != INVALID_HANDLE_VALUE;
  }

  void stop() override {
    running_ = false;
    if (iocp_ && iocp_ != INVALID_HANDLE_VALUE) PostQueuedCompletionStatus(iocp_, 0, 0, nullptr);
    if (th_.joinable()) th_.join();
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& t : targets_) {
      if (t.dir != INVALID_HANDLE_VALUE) CloseHandle(t.dir);
    }
    targets_.clear();
    if (iocp_ && iocp_ != INVALID_HANDLE_VALUE) {
      CloseHandle(iocp_);
      iocp_ = nullptr;
    }
  }

  void add_root(const std::string& root) override {
    std::lock_guard<std::mutex> lock(mu_);
    if (!iocp_) iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 1);
    WatchTarget t;
    t.root = root;
    t.buf.resize(64 * 1024);
    auto w = utf8_to_wide(root);
    t.dir = CreateFileW(w.c_str(), FILE_LIST_DIRECTORY,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    if (t.dir == INVALID_HANDLE_VALUE) {
      log_warn("watch", "could not watch " + root);
      return;
    }
    CreateIoCompletionPort(t.dir, iocp_, reinterpret_cast<ULONG_PTR>(t.dir), 1);
    issue(t);
    targets_.push_back(std::move(t));
  }

private:
  void issue(WatchTarget& t) {
    DWORD rec = 0;
    ReadDirectoryChangesW(t.dir, t.buf.data(), static_cast<DWORD>(t.buf.size()), TRUE,
                          FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                              FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE |
                              FILE_NOTIFY_CHANGE_ATTRIBUTES,
                          &rec, &t.ov, nullptr);
  }

  void loop() {
    while (running_) {
      DWORD bytes = 0;
      ULONG_PTR key = 0;
      LPOVERLAPPED ov = nullptr;
      BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &ov, 200);
      if (!ok || !running_) continue;
      if (!ov || bytes == 0) continue;
      WatchTarget* tgt = nullptr;
      {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& t : targets_)
          if (reinterpret_cast<ULONG_PTR>(t.dir) == key) tgt = &t;
      }
      if (!tgt) continue;
      auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(tgt->buf.data());
      for (;;) {
        std::wstring name(info->FileName, info->FileNameLength / sizeof(WCHAR));
        std::string rel = wide_to_utf8(name);
        std::string full = path_join(tgt->root, rel);
        FsEvent ev;
        ev.path = full;
        switch (info->Action) {
          case FILE_ACTION_ADDED:
            ev.kind = FsEventKind::Created;
            break;
          case FILE_ACTION_REMOVED:
            ev.kind = FsEventKind::Deleted;
            break;
          case FILE_ACTION_MODIFIED:
            ev.kind = FsEventKind::Modified;
            break;
          case FILE_ACTION_RENAMED_OLD_NAME:
            ev.kind = FsEventKind::Renamed;
            pending_old_ = full;
            break;
          case FILE_ACTION_RENAMED_NEW_NAME:
            ev.kind = FsEventKind::Renamed;
            ev.path = pending_old_;
            ev.new_path = full;
            pending_old_.clear();
            break;
          default:
            ev.kind = FsEventKind::Modified;
            break;
        }
        if (info->Action != FILE_ACTION_RENAMED_OLD_NAME && cb_) {
          debounce(ev);
        }
        if (info->NextEntryOffset == 0) break;
        info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(reinterpret_cast<char*>(info) +
                                                          info->NextEntryOffset);
      }
      issue(*tgt);
    }
  }

  void debounce(const FsEvent& ev) {
    auto now = std::chrono::steady_clock::now();
    std::string key = ev.path + "|" + ev.new_path;
    auto& last = last_emit_[key];
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last).count() < debounce_ms_)
      return;
    last = now;
    cb_(ev);
  }

  FsEventFn cb_;
  int debounce_ms_{80};
  std::atomic<bool> running_{false};
  std::thread th_;
  HANDLE iocp_{nullptr};
  std::mutex mu_;
  std::deque<WatchTarget> targets_;
  std::string pending_old_;
  std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_emit_;
};

std::unique_ptr<WatcherBackend> create_watcher_backend() {
  return std::make_unique<WinWatcher>();
}

#else
std::unique_ptr<WatcherBackend> create_watcher_backend();
#endif

}  // namespace wilfred
