#include "wilfred/search/expander.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/snippets.hpp"

#include <cstring>
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
        if (g_ctx->buffer.size() > 128)
          g_ctx->buffer.erase(0, g_ctx->buffer.size() - 128);
        // New word after delimiter: keep buffer for next match but cap it.
      }
    } else if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) {
      bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
      char c = (char)vk;
      if (!shift && c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
      g_ctx->buffer.push_back(c);
      if (g_ctx->buffer.size() > 128) g_ctx->buffer.erase(0, 1);
    } else if (vk == VK_ESCAPE || vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LMENU ||
               vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN || vk == VK_LEFT || vk == VK_RIGHT ||
               vk == VK_UP || vk == VK_DOWN) {
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

bool GlobalExpander::start(const Config& cfg, SnippetStore* snippets) {
  if (!cfg.snippets.global_expansion) return false;
  // macOS/Linux: global key interception needs accessibility / XInput hooks
  // that vary per desktop. The overlay + paste path already works; log once.
  running_ = true;
  snippets_ = snippets;
  cfg_ = &cfg;
  return true;
}

void GlobalExpander::stop() { running_ = false; }

}  // namespace wilfred
#endif
