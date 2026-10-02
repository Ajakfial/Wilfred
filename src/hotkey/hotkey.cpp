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
bool GlobalHotkey::start_binding(const std::vector<std::string>& modifiers, const std::string& key,
                                 bool use_command_on_macos, HotkeyFn cb) {
  return impl_ && impl_->b
             ? impl_->b->start_binding(modifiers, key, use_command_on_macos, std::move(cb))
             : false;
}
void GlobalHotkey::stop() {
  if (impl_ && impl_->b) impl_->b->stop();
}

}  // namespace wilfred
