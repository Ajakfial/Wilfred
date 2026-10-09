#include "wilfred/search/expander.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/snippets.hpp"

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace wilfred {
namespace {

struct HookCtx {
  const Config* cfg{nullptr};
  SnippetStore* store{nullptr};
  std::string buffer;
  HHOOK hook{nullptr};
  DWORD hook_thread{0};
};

static HookCtx* g_ctx = nullptr;

void send_backspaces(std::size_t n) {
  if (!n) return;
  std::vector<INPUT> in;
  in.reserve(n * 2);
  for (std::size_t i = 0; i < n; ++i) {
    INPUT d{};
    d.type = INPUT_KEYBOARD;
    d.ki.wVk = VK_BACK;
    in.push_back(d);
    INPUT u{};
    u.type = INPUT_KEYBOARD;
    u.ki.wVk = VK_BACK;
    u.ki.dwFlags = KEYEVENTF_KEYUP;
    in.push_back(u);
  }
  SendInput((UINT)in.size(), in.data(), sizeof(INPUT));
}

LRESULT CALLBACK ll_keyboard(int code, WPARAM wp, LPARAM lp) {
  if (code == HC_ACTION && g_ctx && (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN)) {
    auto* ks = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);
    DWORD vk = ks->vkCode;
    // Track printable ASCII + backspace; reset on navigation/control keys.
    if (vk == VK_BACK) {
      if (!g_ctx->buffer.empty()) g_ctx->buffer.pop_back();
    } else if (vk == VK_SPACE || vk == VK_TAB || vk == VK_RETURN) {
      char delim = vk == VK_SPACE ? ' ' : (vk == VK_TAB ? '\t' : '\n');
      g_ctx->buffer.push_back(delim);
      std::size_t trig_len = 0;
      const Snippet* hit = snippet_global_match(*g_ctx->store, g_ctx->buffer, trig_len);
      if (hit && trig_len > 0) {
        std::string body = hit->body;
        // Expand placeholders with live clipboard.
        std::string clip;
        try {
          clip = read_clipboard().text;
        } catch (...) {
        }
        body = expand_snippet_placeholders(body, "", clip);
        // Erase trigger + delimiter, paste expansion.
        send_backspaces(trig_len + 1);
        write_clipboard(body);
        // Small delay so backspaces land before Ctrl+V.
        Sleep(30);
        native_simulate_paste();
        g_ctx->buffer.clear();
      } else {
        // Keep a bounded rolling buffer.
        if (g_ctx->buffer.size() > 128) g_ctx->buffer.erase(0, g_ctx->buffer.size() - 128);
        // New word after delimiter: keep buffer for next match but cap it.
      }
    } else if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
      bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
      char c = (char)vk;
      if (!shift && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
      g_ctx->buffer.push_back(c);
      if (g_ctx->buffer.size() > 128) g_ctx->buffer.erase(0, 1);
    } else if (vk == VK_ESCAPE || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LMENU ||
               vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN || vk == VK_LEFT ||
               vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN) {
      g_ctx->buffer.clear();
    }
  }
  return CallNextHookEx(nullptr, code, wp, lp);
}

DWORD WINAPI hook_thread_fn(LPVOID p) {
  auto* ctx = reinterpret_cast<HookCtx*>(p);
  ctx->hook_thread = GetCurrentThreadId();
  ctx->hook = SetWindowsHookExW(WH_KEYBOARD_LL, ll_keyboard, GetModuleHandleW(nullptr), 0);
  if (!ctx->hook) return 1;
  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
  return 0;
}

}  // namespace

bool GlobalExpander::start(const Config& cfg, SnippetStore* snippets) {
  if (running_) return true;
  if (!cfg.snippets.global_expansion || !snippets) return false;
  snippets_ = snippets;
  cfg_ = &cfg;
  auto* ctx = new HookCtx();
  ctx->cfg = &cfg;
  ctx->store = snippets;
  g_ctx = ctx;
  DWORD tid = 0;
  HANDLE th = CreateThread(nullptr, 0, hook_thread_fn, ctx, 0, &tid);
  if (!th) {
    delete ctx;
    g_ctx = nullptr;
    return false;
  }
  thread_ = th;
  thread_id_ = (unsigned long)tid;
  running_ = true;
  // Keep the ctx alive for the process lifetime (freed on stop).
  log_info("snippets", "global expansion enabled (Windows hook)");
  return true;
}

void GlobalExpander::stop() {
  if (!running_) return;
  running_ = false;
  if (g_ctx && g_ctx->hook) UnhookWindowsHookEx(g_ctx->hook);
  if (thread_) {
    PostThreadMessageW((DWORD)thread_id_, WM_QUIT, 0, 0);
    WaitForSingleObject(thread_, 800);
    CloseHandle(thread_);
    thread_ = nullptr;
  }
  delete g_ctx;
  g_ctx = nullptr;
}

}  // namespace wilfred
#else
namespace wilfred {

#if defined(__APPLE__) && !defined(WILFRED_IOS)
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

namespace {
struct MacExpanderCtx {
  SnippetStore* store{nullptr};
  std::string buffer;
  CFMachPortRef tap{nullptr};
  CFRunLoopSourceRef src{nullptr};
  CFRunLoopRef loop{nullptr};
};
static MacExpanderCtx* g_mac_ctx = nullptr;

void mac_send_backspaces(std::size_t n) {
  CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
  if (!src) return;
  for (std::size_t i = 0; i < n; ++i) {
    CGEventRef d = CGEventCreateKeyboardEvent(src, kVK_Delete, true);
    CGEventRef u = CGEventCreateKeyboardEvent(src, kVK_Delete, false);
    if (d) CGEventPost(kCGHIDEventTap, d);
    if (u) CGEventPost(kCGHIDEventTap, u);
    if (d) CFRelease(d);
    if (u) CFRelease(u);
  }
  CFRelease(src);
}

CGEventRef mac_tap_cb(CGEventTapProxy, CGEventType type, CGEventRef ev, void*) {
  if (!g_mac_ctx) return ev;
  if (type != kCGEventKeyDown) return ev;
  UniChar buf[8];
  UniCharCount n = 0;
  CGEventKeyboardGetUnicodeString(ev, 8, &n, buf);
  if (n == 0) {
    // Non-printable (arrows/modifiers): reset word tracking.
    CGKeyCode kc = (CGKeyCode)CGEventGetIntegerValueField(ev, kCGKeyboardEventKeycode);
    if (kc == kVK_Escape || kc == kVK_LeftArrow || kc == kVK_RightArrow || kc == kVK_UpArrow ||
        kc == kVK_DownArrow)
      g_mac_ctx->buffer.clear();
    return ev;
  }
  char c = static_cast<char>(buf[0] & 0x7f);
  if (c == 0x08) {
    if (!g_mac_ctx->buffer.empty()) g_mac_ctx->buffer.pop_back();
    return ev;
  }
  if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
    g_mac_ctx->buffer.push_back(c == '\t' ? '\t' : (c == ' ' ? ' ' : '\n'));
    std::size_t trig = 0;
    const Snippet* hit = snippet_global_match(*g_mac_ctx->store, g_mac_ctx->buffer, trig);
    if (hit && trig > 0 && g_mac_ctx->store) {
      std::string clip;
      try {
        clip = read_clipboard().text;
      } catch (...) {
      }
      auto body = expand_snippet_placeholders(hit->body, "", clip);
      mac_send_backspaces(trig + 1);
      write_clipboard(body);
      native_simulate_paste();
      g_mac_ctx->buffer.clear();
    } else if (g_mac_ctx->buffer.size() > 128) {
      g_mac_ctx->buffer.erase(0, g_mac_ctx->buffer.size() - 128);
    }
    return ev;
  }
  if (c >= 32 && c < 127) {
    g_mac_ctx->buffer.push_back(c);
    if (g_mac_ctx->buffer.size() > 128) g_mac_ctx->buffer.erase(0, 1);
  }
  return ev;
}
}  // namespace

bool GlobalExpander::start(const Config& cfg, SnippetStore* snippets) {
  if (running_) return true;
  if (!cfg.snippets.global_expansion || !snippets) return false;
  snippets_ = snippets;
  cfg_ = &cfg;
  auto* ctx = new MacExpanderCtx();
  ctx->store = snippets;
  g_mac_ctx = ctx;
  // Requires Accessibility permission; creation fails gracefully otherwise.
  ctx->tap = CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
                              CGEventMaskBit(kCGEventKeyDown), mac_tap_cb, nullptr);
  if (!ctx->tap) {
    log_warn("snippets",
             "global expansion needs Accessibility permission (System Settings > Privacy)");
    delete ctx;
    g_mac_ctx = nullptr;
    return false;
  }
  ctx->src = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, ctx->tap, 0);
  ctx->loop = CFRunLoopGetCurrent();
  CFRunLoopAddSource(CFRunLoopGetCurrent(), ctx->src, kCFRunLoopCommonModes);
  CGEventTapEnable(ctx->tap, true);
  running_ = true;
  log_info("snippets", "global expansion enabled (macOS event tap)");
  // Pump the run loop on a detached thread; stop() tears it down.
  std::thread([ctx] {
    CFRunLoopRun();
    (void)ctx;
  }).detach();
  return true;
}

void GlobalExpander::stop() {
  if (!running_) return;
  running_ = false;
  if (g_mac_ctx) {
    if (g_mac_ctx->tap) {
      CGEventTapEnable(g_mac_ctx->tap, false);
      CFMachPortInvalidate(g_mac_ctx->tap);
      CFRelease(g_mac_ctx->tap);
    }
    if (g_mac_ctx->src) CFRelease(g_mac_ctx->src);
    delete g_mac_ctx;
    g_mac_ctx = nullptr;
  }
}

#else

// Linux/BSD X11 backend (Wayland: best-effort via XWayland; native Wayland
// needs a compositor helper — overlay paste still works everywhere).
#if !defined(_WIN32) && !defined(__APPLE__)
#if defined(WILFRED_HAS_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#endif
#include <atomic>
#include <thread>
#endif

namespace {
#if !defined(_WIN32) && !defined(__APPLE__) && defined(WILFRED_HAS_X11)
struct X11ExpanderCtx {
  SnippetStore* store{nullptr};
  std::string buffer;
  Display* dpy{nullptr};
  std::atomic<bool> run{false};
  std::thread th;
};
static X11ExpanderCtx* g_x11_ctx = nullptr;

void x11_send_keys(Display* dpy, Window win, KeySym sym, int count, unsigned int state = 0) {
  if (!dpy || win == None) return;
  KeyCode kc = XKeysymToKeycode(dpy, sym);
  if (!kc) return;
  for (int i = 0; i < count; ++i) {
    XKeyEvent ev{};
    ev.display = dpy;
    ev.window = win;
    ev.root = DefaultRootWindow(dpy);
    ev.subwindow = None;
    ev.time = CurrentTime;
    ev.same_screen = True;
    ev.keycode = kc;
    ev.state = state;
    ev.type = KeyPress;
    XSendEvent(dpy, win, True, KeyPressMask, reinterpret_cast<XEvent*>(&ev));
    ev.type = KeyRelease;
    XSendEvent(dpy, win, True, KeyReleaseMask, reinterpret_cast<XEvent*>(&ev));
  }
  XFlush(dpy);
}
#endif
}  // namespace

bool GlobalExpander::start(const Config& cfg, SnippetStore* snippets) {
  if (running_) return true;
  if (!cfg.snippets.global_expansion || !snippets) return false;
  snippets_ = snippets;
  cfg_ = &cfg;
#if !defined(_WIN32) && !defined(__APPLE__) && defined(WILFRED_HAS_X11)
  auto* ctx = new X11ExpanderCtx();
  ctx->store = snippets;
  ctx->dpy = XOpenDisplay(nullptr);
  if (!ctx->dpy) {
    log_warn("snippets", "global expansion needs X11 (Wayland: use overlay paste)");
    delete ctx;
    return false;
  }
  // Listen for key presses on the root window (X11/XWayland).
  Window root = DefaultRootWindow(ctx->dpy);
  XSelectInput(ctx->dpy, root, KeyPressMask);
  ctx->run = true;
  g_x11_ctx = ctx;
  ctx->th = std::thread([ctx] {
    char text[32];
    KeySym sym = NoSymbol;
    while (ctx->run) {
      if (XPending(ctx->dpy)) {
        XEvent ev;
        XNextEvent(ctx->dpy, &ev);
        if (ev.type == KeyPress) {
          int n = XLookupString(&ev.xkey, text, sizeof(text), &sym, nullptr);
          if (n == 1) {
            char c = text[0];
            if (c == 0x08 || c == 0x7f) {
              if (!ctx->buffer.empty()) ctx->buffer.pop_back();
            } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
              ctx->buffer.push_back(c == ' ' ? ' ' : (c == '\t' ? '\t' : '\n'));
              std::size_t trig = 0;
              const Snippet* hit = snippet_global_match(*ctx->store, ctx->buffer, trig);
              if (hit && trig > 0) {
                std::string clip;
                try {
                  clip = read_clipboard().text;
                } catch (...) {
                }
                auto body = expand_snippet_placeholders(hit->body, "", clip);
                Window focus = None;
                int rev = 0;
                XGetInputFocus(ctx->dpy, &focus, &rev);
                x11_send_keys(ctx->dpy, focus, XK_BackSpace, static_cast<int>(trig + 1));
                write_clipboard(body);
                // Ctrl+V to the focused window.
                x11_send_keys(ctx->dpy, focus, XK_Control_L, 0);
                XKeyEvent cev{};
                cev.display = ctx->dpy;
                cev.window = focus;
                cev.root = DefaultRootWindow(ctx->dpy);
                cev.time = CurrentTime;
                cev.same_screen = True;
                KeyCode vkc = XKeysymToKeycode(ctx->dpy, XK_v);
                cev.keycode = vkc;
                cev.state = ControlMask;
                cev.type = KeyPress;
                XSendEvent(ctx->dpy, focus, True, KeyPressMask, reinterpret_cast<XEvent*>(&cev));
                cev.type = KeyRelease;
                XSendEvent(ctx->dpy, focus, True, KeyReleaseMask, reinterpret_cast<XEvent*>(&cev));
                XFlush(ctx->dpy);
                ctx->buffer.clear();
              } else if (ctx->buffer.size() > 128) {
                ctx->buffer.erase(0, ctx->buffer.size() - 128);
              }
            } else if (c >= 32 && c < 127) {
              ctx->buffer.push_back(c);
              if (ctx->buffer.size() > 128) ctx->buffer.erase(0, 1);
            }
          } else {
            ctx->buffer.clear();
          }
        }
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
      }
    }
  });
  running_ = true;
  log_info("snippets", "global expansion enabled (X11)");
  return true;
#else
  // No X11 (Wayland-native, BSD console, mobile): there is no cross-desktop
  // key hook. Report unsupported so callers show overlay paste instead of
  // pretending the global hook is live.
  log_warn("snippets", "global expansion needs X11 (Wayland-native: use overlay paste)");
  return false;
#endif
}

void GlobalExpander::stop() {
  if (!running_) return;
  running_ = false;
#if !defined(_WIN32) && !defined(__APPLE__) && defined(WILFRED_HAS_X11)
  if (g_x11_ctx) {
    g_x11_ctx->run = false;
    if (g_x11_ctx->th.joinable()) g_x11_ctx->th.join();
    if (g_x11_ctx->dpy) XCloseDisplay(g_x11_ctx->dpy);
    delete g_x11_ctx;
    g_x11_ctx = nullptr;
  }
#endif
}

#endif  // __APPLE__ check

}  // namespace wilfred
#endif
