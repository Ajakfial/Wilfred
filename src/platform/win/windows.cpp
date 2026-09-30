#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <dwmapi.h>
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>
#endif

namespace wilfred {
#ifdef _WIN32

namespace {

bool window_is_cloaked(HWND hwnd) {
  BOOL cloaked = FALSE;
  if (FAILED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) return false;
  return cloaked != FALSE;
}

bool is_alt_tab_window(HWND hwnd) {
  if (!IsWindowVisible(hwnd)) return false;
  if (GetWindow(hwnd, GW_OWNER)) return false;
  LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
  if (ex & WS_EX_TOOLWINDOW) return false;
  if (window_is_cloaked(hwnd)) return false;
  wchar_t cls[64]{};
  GetClassNameW(hwnd, cls, 64);
  if (lstrcmpW(cls, L"WilfredOverlay") == 0) return false;
  wchar_t title[512]{};
  if (GetWindowTextW(hwnd, title, 512) <= 0 || !title[0]) return false;
  return true;
}

std::string process_name_for_pid(DWORD pid) {
  HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!h) return {};
  wchar_t path[MAX_PATH]{};
  DWORD n = MAX_PATH;
  std::string name;
  if (QueryFullProcessImageNameW(h, 0, path, &n)) name = path_filename(wide_to_utf8(path));
  CloseHandle(h);
  return name;
}

struct EnumCtx {
  std::vector<NativeWindowInfo>* out;
};

BOOL CALLBACK enum_windows_cb(HWND hwnd, LPARAM lp) {
  auto* ctx = reinterpret_cast<EnumCtx*>(lp);
  if (!is_alt_tab_window(hwnd)) return TRUE;
  wchar_t title[512]{};
  GetWindowTextW(hwnd, title, 512);
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  NativeWindowInfo w;
  w.id = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(hwnd));
  w.title = wide_to_utf8(title);
  w.owner = process_name_for_pid(pid);
  ctx->out->push_back(std::move(w));
  return TRUE;
}

}  // namespace

std::vector<NativeWindowInfo> native_list_windows() {
  std::vector<NativeWindowInfo> out;
  EnumCtx ctx{&out};
  EnumWindows(enum_windows_cb, reinterpret_cast<LPARAM>(&ctx));
  return out;
}

bool native_focus_window(std::uint64_t id) {
  if (!id) return false;
  HWND hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(id));
  if (!IsWindow(hwnd)) return false;

  HWND overlay = FindWindowW(L"WilfredOverlay", nullptr);
  if (overlay && overlay != hwnd) ShowWindow(overlay, SW_HIDE);

  if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);

  HWND fg = GetForegroundWindow();
  DWORD cur = GetCurrentThreadId();
  DWORD fg_tid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
  if (fg_tid && fg_tid != cur) AttachThreadInput(cur, fg_tid, TRUE);
  BringWindowToTop(hwnd);
  SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
  BOOL ok = SetForegroundWindow(hwnd);
  if (fg_tid && fg_tid != cur) AttachThreadInput(cur, fg_tid, FALSE);
  if (!ok) {
    DWORD self = 0;
    GetWindowThreadProcessId(hwnd, &self);
    AllowSetForegroundWindow(self);
    SetForegroundWindow(hwnd);
  }
  return GetForegroundWindow() == hwnd || IsWindowVisible(hwnd);
}

#endif
}  // namespace wilfred
