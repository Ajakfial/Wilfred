#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
// windows.h first: the registry API below needs its types.
#include <windows.h>

#include <string>
#endif

namespace wilfred {
#ifdef _WIN32

// Autostart via HKCU\...\Run (per-user, no elevation). The value holds the
// quoted exe path; bare `wilfred` runs the daemon.
namespace {
const wchar_t* kRunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const wchar_t* kValueName = L"Wilfred";
}  // namespace

bool native_autostart_status(bool& enabled, std::string& detail, std::string& error) {
  enabled = false;
  detail.clear();
  HKEY key = nullptr;
  LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &key);
  if (rc != ERROR_SUCCESS) {
    // Treat any registry access error as "not enabled" rather than failure.
    // This can happen in restricted environments (CI, sandboxed apps, etc.).
    error.clear();  // Not a real error, just unavailable.
    return true;
  }
  wchar_t buf[1024];
  DWORD n = sizeof(buf);
  DWORD type = 0;
  rc = RegQueryValueExW(key, kValueName, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &n);
  RegCloseKey(key);
  if (rc == ERROR_FILE_NOT_FOUND) return true;
  if (rc != ERROR_SUCCESS || type != REG_SZ) {
    // Value unreadable or wrong type - treat as not enabled.
    error.clear();
    return true;
  }
  detail = wide_to_utf8(std::wstring(buf, n / sizeof(wchar_t)));
  while (!detail.empty() && detail.back() == '\0') detail.pop_back();
  enabled = (detail == "\"" + exe_path() + "\"");
  return true;
}

bool native_autostart_set(bool enable, std::string& error) {
  std::string exe = exe_path();
  if (enable && exe.empty()) {
    error = "cannot determine executable path";
    return false;
  }
  HKEY key = nullptr;
  LONG rc = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key);
  if (rc != ERROR_SUCCESS) {
    error = "cannot open HKCU Run key for writing";
    return false;
  }
  bool ok = true;
  if (enable) {
    std::wstring w = utf8_to_wide("\"" + exe + "\"");
    rc = RegSetValueExW(key, kValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(w.c_str()),
                        static_cast<DWORD>((w.size() + 1) * sizeof(wchar_t)));
    if (rc != ERROR_SUCCESS) {
      error = "cannot write Wilfred Run value";
      ok = false;
    }
  } else {
    rc = RegDeleteValueW(key, kValueName);
    if (rc != ERROR_SUCCESS && rc != ERROR_FILE_NOT_FOUND) {
      error = "cannot delete Wilfred Run value";
      ok = false;
    }
  }
  RegCloseKey(key);
  return ok;
}

#endif
}  // namespace wilfred
