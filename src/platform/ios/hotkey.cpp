#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

namespace wilfred {
#if defined(WILFRED_IOS)

// iOS offers no global-hotkey API to sandboxed apps. The app icon is the
// entry point (like the Android floating W, but without overlay windows,
// which iOS forbids); summoning therefore lives entirely in Swift.

class IosHotkey final : public HotkeyBackend {
 public:
  bool start(const Config&, HotkeyFn) override { return false; }
  bool start_binding(const std::vector<std::string>&, const std::string&, bool,
                     HotkeyFn) override {
    return false;
  }
  void stop() override {}
};

std::unique_ptr<HotkeyBackend> create_hotkey_backend() {
  return std::make_unique<IosHotkey>();
}

#endif
}  // namespace wilfred
