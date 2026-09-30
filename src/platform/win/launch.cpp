#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <powrprof.h>
#endif

namespace wilfred {
#ifdef _WIN32

bool native_launch(const std::string& path) {
  auto w = utf8_to_wide(path);
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  return rc > 32;
}

bool native_reveal(const std::string& path) {
  auto w = utf8_to_wide(path);
  std::wstring args = L"/select,\"" + w + L"\"";
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL));
  return rc > 32;
}

bool native_open_url(const std::string& url) {
  auto w = utf8_to_wide(url);
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", w.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
  return rc > 32;
}

static bool run_hidden(const wchar_t* exe, const wchar_t* args) {
  auto rc = reinterpret_cast<INT_PTR>(
      ShellExecuteW(nullptr, L"open", exe, args, nullptr, SW_HIDE));
  return rc > 32;
}

static void enable_shutdown_privilege() {
  HANDLE tok = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &tok))
    return;
  TOKEN_PRIVILEGES tp{};
  tp.PrivilegeCount = 1;
  tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  if (LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid))
    AdjustTokenPrivileges(tok, FALSE, &tp, sizeof(tp), nullptr, nullptr);
  CloseHandle(tok);
}

bool native_system_action(const std::string& id) {
  if (id == "lock") return LockWorkStation() != 0;
  if (id == "sleep") {
    enable_shutdown_privilege();
    return SetSuspendState(FALSE, FALSE, FALSE) != FALSE;
  }
  if (id == "shutdown") return run_hidden(L"shutdown.exe", L"/s /t 0");
  if (id == "restart") return run_hidden(L"shutdown.exe", L"/r /t 0");
  if (id == "logout") {
    enable_shutdown_privilege();
    return ExitWindowsEx(EWX_LOGOFF, 0) != 0;
  }
  if (id == "empty_trash") {
    auto hr = SHEmptyRecycleBinW(nullptr, nullptr,
                                 SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
    return SUCCEEDED(hr) || hr == S_FALSE;
  }
  return false;
}

#else
bool native_launch(const std::string&);
#endif
}  // namespace wilfred
