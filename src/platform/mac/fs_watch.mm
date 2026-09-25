#include "wilfred/platform/native.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"

#import <CoreServices/CoreServices.h>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

namespace wilfred {

#ifdef __APPLE__

class MacWatcher final : public WatcherBackend {
public:
  ~MacWatcher() override { stop(); }

  bool start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb) override {
    cb_ = std::move(cb);
    debounce_ms_ = debounce_ms;
    roots_ = roots;
    FSEventStreamContext ctx{};
    ctx.info = this;
    CFMutableArrayRef paths = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    for (auto& r : roots_) {
      CFStringRef s = CFStringCreateWithCString(kCFAllocatorDefault, r.c_str(), kCFStringEncodingUTF8);
      CFArrayAppendValue(paths, s);
      CFRelease(s);
    }
    stream_ = FSEventStreamCreate(kCFAllocatorDefault, &MacWatcher::callback, &ctx, paths,
                                  kFSEventStreamEventIdSinceNow, debounce_ms_ / 1000.0,
                                  kFSEventStreamCreateFlagFileEvents);
    CFRelease(paths);
    if (!stream_) return false;
    FSEventStreamScheduleWithRunLoop(stream_, CFRunLoopGetMain(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream_);
    running_ = true;
    return true;
  }

  void stop() override {
    running_ = false;
    if (stream_) {
      FSEventStreamStop(stream_);
      FSEventStreamInvalidate(stream_);
      FSEventStreamRelease(stream_);
      stream_ = nullptr;
    }
  }

  void add_root(const std::string& root) override {
    roots_.push_back(root);
    stop();
    start(roots_, debounce_ms_, cb_);
  }

  static void callback(ConstFSEventStreamRef, void* info, size_t count, void* paths,
                       const FSEventStreamEventFlags flags[], const FSEventStreamEventId[]) {
    auto* self = static_cast<MacWatcher*>(info);
    auto** p = static_cast<char**>(paths);
    for (size_t i = 0; i < count; ++i) {
      FsEvent ev;
      ev.path = p[i] ? p[i] : "";
      if (flags[i] & kFSEventStreamEventFlagItemRemoved)
        ev.kind = FsEventKind::Deleted;
      else if (flags[i] & kFSEventStreamEventFlagItemRenamed)
        ev.kind = FsEventKind::Renamed;
      else if (flags[i] & kFSEventStreamEventFlagItemCreated)
        ev.kind = FsEventKind::Created;
      else
        ev.kind = FsEventKind::Modified;
      if (self->cb_) self->cb_(ev);
    }
  }

private:
  FsEventFn cb_;
  int debounce_ms_{80};
  std::vector<std::string> roots_;
  FSEventStreamRef stream_{nullptr};
  std::atomic<bool> running_{false};
};

std::unique_ptr<WatcherBackend> create_watcher_backend() { return std::make_unique<MacWatcher>(); }

#endif
}  // namespace wilfred
