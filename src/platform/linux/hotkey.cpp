#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#include <atomic>
#include <chrono>
#include <thread>

#if !defined(_WIN32) && !defined(__APPLE__)
#if defined(WILFRED_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif
#endif

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)

class LinuxHotkey final : public HotkeyBackend {
public:
  ~LinuxHotkey() override { stop(); }
  bool start(const Config& cfg, HotkeyFn cb) override {
    return start_binding(cfg.hotkey.modifiers, cfg.hotkey.key, false, std::move(cb));
  }
  bool start_binding(const std::vector<std::string>& modifiers, const std::string& key,
                     bool /*use_command_on_macos*/, HotkeyFn cb) override {
#ifdef WILFRED_HAS_X11
    stop();
    cb_ = std::move(cb);
    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) return false;
    unsigned int mods = 0;
    for (auto& m : modifiers) {
      auto l = to_lower_utf8(m);
      if (l == "ctrl" || l == "control")
        mods |= ControlMask;
      else if (l == "alt")
        mods |= Mod1Mask;
      else if (l == "shift")
        mods |= ShiftMask;
      else if (l == "super" || l == "win" || l == "meta")
        mods |= Mod4Mask;
    }
    KeySym ks = XStringToKeysym(key.c_str());
    if (ks == NoSymbol) ks = XStringToKeysym(to_lower_utf8(key).c_str());
    if (ks == NoSymbol && !key.empty())
      ks = static_cast<KeySym>(std::tolower(static_cast<unsigned char>(key[0])));
    key_ = XKeysymToKeycode(dpy_, ks);
    mods_ = mods;
    Window root = DefaultRootWindow(dpy_);
    XGrabKey(dpy_, key_, mods, root, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(dpy_, key_, mods | LockMask, root, True, GrabModeAsync, GrabModeAsync);
    XSelectInput(dpy_, root, KeyPressMask);
    running_ = true;
    th_ = std::thread([this] {
      while (running_) {
        if (XPending(dpy_)) {
          XEvent ev;
          XNextEvent(dpy_, &ev);
          if (ev.type == KeyPress && cb_) cb_();
        } else {
          std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
      }
    });
    return true;
#else
    (void)modifiers;
    (void)key;
    (void)cb;
    return false;
#endif
  }
  void stop() override {
    running_ = false;
#ifdef WILFRED_HAS_X11
    if (dpy_) XUngrabKey(dpy_, key_, mods_, DefaultRootWindow(dpy_));
    if (th_.joinable()) th_.join();
    if (dpy_) {
      XCloseDisplay(dpy_);
      dpy_ = nullptr;
    }
#else
    if (th_.joinable()) th_.join();
#endif
  }

private:
  HotkeyFn cb_;
#ifdef WILFRED_HAS_X11
  Display* dpy_{nullptr};
  KeyCode key_{0};
  unsigned int mods_{0};
#endif
  std::atomic<bool> running_{false};
  std::thread th_;
};

std::unique_ptr<HotkeyBackend> create_hotkey_backend() {
  return std::make_unique<LinuxHotkey>();
}

#endif
}  // namespace wilfred
