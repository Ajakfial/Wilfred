#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#include <atomic>
#include <cctype>
#include <thread>

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

namespace wilfred {
#ifdef __APPLE__

static UInt32 mac_vk(const std::string& key) {
  if (key.empty()) return kVK_ANSI_W;
  char c = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
  switch (c) {
    case 'A':
      return kVK_ANSI_A;
    case 'B':
      return kVK_ANSI_B;
    case 'C':
      return kVK_ANSI_C;
    case 'D':
      return kVK_ANSI_D;
    case 'E':
      return kVK_ANSI_E;
    case 'F':
      return kVK_ANSI_F;
    case 'G':
      return kVK_ANSI_G;
    case 'H':
      return kVK_ANSI_H;
    case 'I':
      return kVK_ANSI_I;
    case 'J':
      return kVK_ANSI_J;
    case 'K':
      return kVK_ANSI_K;
    case 'L':
      return kVK_ANSI_L;
    case 'M':
      return kVK_ANSI_M;
    case 'N':
      return kVK_ANSI_N;
    case 'O':
      return kVK_ANSI_O;
    case 'P':
      return kVK_ANSI_P;
    case 'Q':
      return kVK_ANSI_Q;
    case 'R':
      return kVK_ANSI_R;
    case 'S':
      return kVK_ANSI_S;
    case 'T':
      return kVK_ANSI_T;
    case 'U':
      return kVK_ANSI_U;
    case 'V':
      return kVK_ANSI_V;
    case 'W':
      return kVK_ANSI_W;
    case 'X':
      return kVK_ANSI_X;
    case 'Y':
      return kVK_ANSI_Y;
    case 'Z':
      return kVK_ANSI_Z;
    case ' ':
      return kVK_Space;
    default:
      return kVK_ANSI_W;
  }
}

class MacHotkey final : public HotkeyBackend {
public:
  ~MacHotkey() override { stop(); }

  bool start(const Config& cfg, HotkeyFn cb) override {
    return start_binding(cfg.hotkey.modifiers, cfg.hotkey.key, cfg.hotkey.use_command_on_macos,
                         std::move(cb));
  }
  bool start_binding(const std::vector<std::string>& modifiers, const std::string& key,
                     bool use_command_on_macos, HotkeyFn cb) override {
    stop();
    cb_ = std::move(cb);
    UInt32 mods = optionKey;
    for (auto& m : modifiers) {
      auto l = to_lower_utf8(m);
      if (l == "ctrl" || l == "control") {
        if (use_command_on_macos)
          mods |= cmdKey;
        else
          mods |= controlKey;
      } else if (l == "cmd" || l == "command" || l == "super" || l == "meta")
        mods |= cmdKey;
      else if (l == "alt" || l == "option")
        mods |= optionKey;
      else if (l == "shift")
        mods |= shiftKey;
    }
    EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
    InstallApplicationEventHandler(&MacHotkey::handler, 1, &spec, this, &handler_);
    // Unique id per instance so extra bindings coexist with the primary one.
    UInt32 slot = s_next_id.fetch_add(1);
    EventHotKeyID hid{static_cast<OSType>('WLFD'), slot};
    OSStatus st =
        RegisterEventHotKey(mac_vk(key), mods, hid, GetApplicationEventTarget(), 0, &hotkey_);
    running_ = st == noErr;
    return running_;
  }

  void stop() override {
    running_ = false;
    if (hotkey_) {
      UnregisterEventHotKey(hotkey_);
      hotkey_ = nullptr;
    }
    if (handler_) {
      RemoveEventHandler(handler_);
      handler_ = nullptr;
    }
  }

  static OSStatus handler(EventHandlerCallRef, EventRef, void* data) {
    auto* self = static_cast<MacHotkey*>(data);
    if (self && self->cb_) self->cb_();
    return noErr;
  }

private:
  HotkeyFn cb_;
  EventHotKeyRef hotkey_{nullptr};
  EventHandlerRef handler_{nullptr};
  std::atomic<bool> running_{false};
  static std::atomic<UInt32> s_next_id;
};

std::atomic<UInt32> MacHotkey::s_next_id{2};

std::unique_ptr<HotkeyBackend> create_hotkey_backend() {
  return std::make_unique<MacHotkey>();
}

#endif
}  // namespace wilfred
