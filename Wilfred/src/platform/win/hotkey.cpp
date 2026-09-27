#include "wilfred/platform/native.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/utf8.hpp"

#include <cctype>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {
#ifdef _WIN32

static UINT vk_from_key(const std::string& key) {
  if (key.size() == 1) {
    char c = static_cast<char>(std::toupper(static_cast<unsigned char>(key[0])));
    return static_cast<UINT>(c);
  }
  auto l = to_lower_utf8(key);
  if (l == "space") return VK_SPACE;
  if (l == "enter") return VK_RETURN;
  if (l == "tab") return VK_TAB;
  if (l == "esc" || l == "escape") return VK_ESCAPE;
  return static_cast<UINT>(std::toupper(static_cast<unsigned char>(key[0])));
}

class WinHotkey final : public HotkeyBackend {
public:
  ~WinHotkey() override { stop(); }
  bool start(const Config& cfg, HotkeyFn cb) override {
    stop();
    cb_ = std::move(cb);
    UINT mods = 0;
    for (auto& m : cfg.hotkey.modifiers) {
      auto l = to_lower_utf8(m);
      if (l == "ctrl" || l == "control") mods |= MOD_CONTROL;
      else if (l == "alt")
        mods |= MOD_ALT;
      else if (l == "shift")
        mods |= MOD_SHIFT;
      else if (l == "win" || l == "super" || l == "meta" || l == "cmd")
        mods |= MOD_WIN;
    }
    vk_ = vk_from_key(cfg.hotkey.key);
    mods_ = mods;

    WNDCLASSW wc{};
    wc.lpfnWndProc = &WinHotkey::wnd_proc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"WilfredHotkeySink";
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
      log_warn("hotkey", "failed to register hotkey window class");
      return false;
    }

    hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                            wc.hInstance, this);
    if (!hwnd_) {
      log_warn("hotkey", "failed to create hotkey message window");
      return false;
    }

    if (!RegisterHotKey(hwnd_, 1, mods_ | MOD_NOREPEAT, vk_)) {
      log_warn("hotkey", "RegisterHotKey failed (is Ctrl+Alt+W already in use?)");
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
      return false;
    }
    log_info("hotkey", "global hotkey registered");
    return true;
  }
  void stop() override {
    if (hwnd_) {
      UnregisterHotKey(hwnd_, 1);
      DestroyWindow(hwnd_);
      hwnd_ = nullptr;
    }
  }

private:
  static LRESULT CALLBACK wnd_proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCCREATE) {
      auto* cs = reinterpret_cast<CREATESTRUCTW*>(l);
      SetWindowLongPtrW(h, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* self = reinterpret_cast<WinHotkey*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (self && m == WM_HOTKEY && self->cb_) {
      self->cb_();
      return 0;
    }
    return DefWindowProcW(h, m, w, l);
  }

  HotkeyFn cb_;
  UINT vk_{0}, mods_{0};
  HWND hwnd_{nullptr};
};

std::unique_ptr<HotkeyBackend> create_hotkey_backend() { return std::make_unique<WinHotkey>(); }

#else
std::unique_ptr<HotkeyBackend> create_hotkey_backend();
#endif
}  // namespace wilfred
