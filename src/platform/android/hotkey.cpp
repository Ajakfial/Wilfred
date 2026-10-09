#include "wilfred/platform/native.hpp"

namespace wilfred {
#if defined(__ANDROID__)

// The floating "W" button replaces the desktop global hotkey. There is no
// system-wide key grab on Android; FloatingWService owns the overlay entry
// point and launches the search UI on tap.

class AndroidHotkey final : public HotkeyBackend {
public:
  bool start(const Config&, HotkeyFn) override { return false; }
  bool start_binding(const std::vector<std::string>&, const std::string&, bool, HotkeyFn) override {
    return false;
  }
  void stop() override {}
};

std::unique_ptr<HotkeyBackend> create_hotkey_backend() {
  return std::make_unique<AndroidHotkey>();
}

#endif
}  // namespace wilfred
