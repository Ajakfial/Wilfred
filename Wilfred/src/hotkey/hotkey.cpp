#include "wilfred/hotkey/hotkey.hpp"

#include "wilfred/platform/native.hpp"

namespace wilfred {

struct GlobalHotkey::Impl {
  std::unique_ptr<HotkeyBackend> b = create_hotkey_backend();
};

GlobalHotkey::GlobalHotkey() : impl_(std::make_unique<Impl>()) {}
GlobalHotkey::~GlobalHotkey() { stop(); }

bool GlobalHotkey::start(const Config& cfg, HotkeyFn cb) {
  return impl_ && impl_->b ? impl_->b->start(cfg, std::move(cb)) : false;
}
void GlobalHotkey::stop() {
  if (impl_ && impl_->b) impl_->b->stop();
}

}  // namespace wilfred
