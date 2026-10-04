#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <dwmapi.h>
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
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

namespace {

bool window_work_area(HWND hwnd, RECT& work) {
  HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi{};
  mi.cbSize = sizeof(mi);
  if (mon && GetMonitorInfoW(mon, &mi)) {
    work = mi.rcWork;
    return true;
  }
  return SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0) != 0;
}

}  // namespace

bool native_window_action(std::uint64_t id, NativeWindowOp op, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  HWND hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(id));
  if (!IsWindow(hwnd)) {
    error = "window no longer exists";
    return false;
  }
  switch (op) {
    case NativeWindowOp::Minimize:
      if (!ShowWindow(hwnd, SW_MINIMIZE)) {
        error = "could not minimize window";
        return false;
      }
      return true;
    case NativeWindowOp::Maximize:
      if (!ShowWindow(hwnd, SW_MAXIMIZE)) {
        error = "could not maximize window";
        return false;
      }
      return true;
    case NativeWindowOp::Restore:
      if (!ShowWindow(hwnd, SW_RESTORE)) {
        error = "could not restore window";
        return false;
      }
      return true;
    case NativeWindowOp::Close:
      if (!PostMessageW(hwnd, WM_CLOSE, 0, 0)) {
        error = "could not close window";
        return false;
      }
      return true;
    case NativeWindowOp::SnapLeft:
    case NativeWindowOp::SnapRight: {
      if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
      RECT work{};
      if (!window_work_area(hwnd, work)) {
        error = "could not read screen work area";
        return false;
      }
      int half = (work.right - work.left) / 2;
      int x = op == NativeWindowOp::SnapLeft ? work.left : work.left + half;
      if (!SetWindowPos(hwnd, nullptr, x, work.top, half, work.bottom - work.top,
                        SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW)) {
        error = "could not snap window";
        return false;
      }
      return true;
    }
  }
  error = "unknown window operation";
  return false;
}

bool native_window_rect(std::uint64_t id, NativeWindowRect& rect, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  HWND hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(id));
  if (!IsWindow(hwnd)) {
    error = "window no longer exists";
    return false;
  }
  RECT r{};
  if (!GetWindowRect(hwnd, &r)) {
    error = "could not read window rect";
    return false;
  }
  rect.x = r.left;
  rect.y = r.top;
  rect.w = r.right - r.left;
  rect.h = r.bottom - r.top;
  rect.maximized = IsZoomed(hwnd) != 0;
  return true;
}

bool native_window_move(std::uint64_t id, int x, int y, int w, int h, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  HWND hwnd = reinterpret_cast<HWND>(static_cast<std::uintptr_t>(id));
  if (!IsWindow(hwnd)) {
    error = "window no longer exists";
    return false;
  }
  if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
  UINT flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW;
  int use_w = w, use_h = h;
  if (use_w <= 0 || use_h <= 0) {
    RECT r{};
    if (!GetWindowRect(hwnd, &r)) {
      error = "could not read window rect";
      return false;
    }
    if (use_w <= 0) use_w = r.right - r.left;
    if (use_h <= 0) use_h = r.bottom - r.top;
  }
  if (!SetWindowPos(hwnd, nullptr, x, y, use_w, use_h, flags)) {
    error = "could not move window";
    return false;
  }
  return true;
}

bool native_primary_work_area(NativeWorkArea& out, std::string& error) {
  RECT rc{};
  if (!SystemParametersInfoW(SPI_GETWORKAREA, 0, &rc, 0)) {
    error = "cannot read work area";
    return false;
  }
  out.x = rc.left;
  out.y = rc.top;
  out.w = rc.right - rc.left;
  out.h = rc.bottom - rc.top;
  if (out.w <= 0 || out.h <= 0) {
    error = "invalid work area";
    return false;
  }
  return true;
}

namespace {

struct MonSigCtx {
  std::vector<std::string>* parts;
};

BOOL CALLBACK mon_sig_cb(HMONITOR, HDC, LPRECT rc, LPARAM lp) {
  auto* ctx = reinterpret_cast<MonSigCtx*>(lp);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%dx%d@%d,%d", rc->right - rc->left, rc->bottom - rc->top,
                rc->left, rc->top);
  ctx->parts->push_back(buf);
  return TRUE;
}

}  // namespace

bool native_monitor_signature(std::string& sig, std::string& error) {
  std::vector<std::string> parts;
  MonSigCtx ctx{&parts};
  if (!EnumDisplayMonitors(nullptr, nullptr, mon_sig_cb, reinterpret_cast<LPARAM>(&ctx)) ||
      parts.empty()) {
    error = "cannot enumerate monitors";
    return false;
  }
  std::sort(parts.begin(), parts.end());
  sig.clear();
  for (std::size_t i = 0; i < parts.size(); ++i) {
    if (i) sig.push_back('|');
    sig += parts[i];
  }
  return true;
}

#endif
}  // namespace wilfred
