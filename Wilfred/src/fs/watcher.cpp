#include "wilfred/fs/watcher.hpp"

#include "wilfred/platform/native.hpp"

namespace wilfred {

struct FsWatcher::Impl {
  std::unique_ptr<WatcherBackend> b = create_watcher_backend();
};

FsWatcher::FsWatcher() : impl_(std::make_unique<Impl>()) {}
FsWatcher::~FsWatcher() { stop(); }

bool FsWatcher::start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb) {
  return impl_ && impl_->b ? impl_->b->start(roots, debounce_ms, std::move(cb)) : false;
}

void FsWatcher::stop() {
  if (impl_ && impl_->b) impl_->b->stop();
}

void FsWatcher::add_root(const std::string& root) {
  if (impl_ && impl_->b) impl_->b->add_root(root);
}

}  // namespace wilfred
