#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <powrprof.h>
#include <winver.h>

#include <cwchar>
#include <vector>
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

namespace {

bool has_exe_ext(const std::wstring& n) {
  return n.size() > 4 && _wcsicmp(n.c_str() + n.size() - 4, L".exe") == 0;
}

std::wstring win_lower(std::wstring s) {
  for (auto& c : s) c = static_cast<wchar_t>(towlower(c));
  return s;
}

bool win_file_exists(const std::wstring& p) {
  DWORD a = GetFileAttributesW(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// Resolve an executable name ("code.exe", "C:\Tools\app.exe") to a full path.
std::wstring resolve_exe_name(const std::wstring& name) {
  if (name.empty()) return {};
  if (name.find(L'\\') != std::wstring::npos || name.find(L'/') != std::wstring::npos) {
    return win_file_exists(name) ? name : std::wstring();
  }
  WCHAR full[MAX_PATH]{};
  DWORD n = SearchPathW(nullptr, name.c_str(), has_exe_ext(name) ? nullptr : L".exe", MAX_PATH,
                        full, nullptr);
  if (n > 0 && n < MAX_PATH) return full;
  for (HKEY base : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    for (int t = 0; t < 2; ++t) {
      std::wstring key =
          L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\" + name +
          ((t == 1 && !has_exe_ext(name)) ? L".exe" : L"");
      HKEY h = nullptr;
      if (RegOpenKeyExW(base, key.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) continue;
      WCHAR data[MAX_PATH]{};
      DWORD dn = sizeof(data);
      DWORD type = 0;
      LONG rc = RegQueryValueExW(h, nullptr, nullptr, &type, reinterpret_cast<BYTE*>(data), &dn);
      RegCloseKey(h);
      if (rc == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && data[0]) {
        if (win_file_exists(data)) return data;
      }
    }
  }
  return {};
}

std::string exe_display_name(const std::wstring& exe) {
  DWORD dummy = 0;
  DWORD sz = GetFileVersionInfoSizeW(exe.c_str(), &dummy);
  if (sz) {
    std::vector<char> buf(sz);
    if (GetFileVersionInfoW(exe.c_str(), 0, sz, buf.data())) {
      struct LangCp {
        WORD lang, cp;
      };
      LangCp* tc = nullptr;
      UINT tl = 0;
      auto query_desc = [&](const wchar_t* sub) -> std::string {
        WCHAR* val = nullptr;
        UINT vl = 0;
        if (VerQueryValueW(buf.data(), sub, reinterpret_cast<void**>(&val), &vl) && vl &&
            val && val[0])
          return wide_to_utf8(val);
        return {};
      };
      if (VerQueryValueW(buf.data(), L"\\VarFileInfo\\Translation",
                         reinterpret_cast<void**>(&tc), &tl) &&
          tl >= sizeof(LangCp)) {
        WCHAR sub[64]{};
        swprintf(sub, 64, L"\\StringFileInfo\\%04x%04x\\FileDescription", tc[0].lang, tc[0].cp);
        if (auto d = query_desc(sub); !d.empty()) return d;
      }
      if (auto d = query_desc(L"\\StringFileInfo\\040904E4\\FileDescription"); !d.empty())
        return d;
    }
  }
  auto u = wide_to_utf8(exe);
  auto b = path_filename(u);
  auto dot = b.rfind('.');
  if (dot != std::string::npos) b.resize(dot);
  return b.empty() ? u : b;
}

// Extract the executable from an open-command string like
// "\"C:\App\app.exe\" \"%1\"" or "C:\App\app.exe %1".
std::wstring exe_from_command(const std::wstring& cmd) {
  std::size_t i = 0;
  while (i < cmd.size() && iswspace(cmd[i])) ++i;
  if (i >= cmd.size()) return {};
  std::wstring cand;
  if (cmd[i] == L'"') {
    auto e = cmd.find(L'"', i + 1);
    cand = cmd.substr(i + 1, e == std::wstring::npos ? std::wstring::npos : e - i - 1);
  } else {
    auto e = cmd.find(L' ', i);
    cand = cmd.substr(i, e == std::wstring::npos ? std::wstring::npos : e - i);
  }
  if (!has_exe_ext(cand)) return {};
  if (cand.find(L'\\') != std::wstring::npos || cand.find(L'/') != std::wstring::npos)
    return win_file_exists(cand) ? cand : std::wstring();
  return resolve_exe_name(cand);
}

std::wstring reg_default_string(HKEY root, const std::wstring& subkey) {
  HKEY h = nullptr;
  if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) return {};
  WCHAR data[1024]{};
  DWORD dn = sizeof(data);
  DWORD type = 0;
  LONG rc = RegQueryValueExW(h, nullptr, nullptr, &type, reinterpret_cast<BYTE*>(data), &dn);
  RegCloseKey(h);
  if (rc != ERROR_SUCCESS || (type != REG_SZ && type != REG_EXPAND_SZ)) return {};
  return data;
}

void collect_openwith_candidates(const std::wstring& ext, std::vector<std::wstring>& exes) {
  for (HKEY base : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    std::wstring key =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext +
        L"\\OpenWithList";
    HKEY h = nullptr;
    if (RegOpenKeyExW(base, key.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) continue;
    for (DWORD i = 0;; ++i) {
      WCHAR vn[256]{};
      DWORD vns = 256;
      BYTE data[1024]{};
      DWORD dns = sizeof(data);
      DWORD type = 0;
      if (RegEnumValueW(h, i, vn, &vns, nullptr, &type, data, &dns) != ERROR_SUCCESS) break;
      if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
      if (_wcsicmp(vn, L"MRUList") == 0) continue;
      exes.push_back(reinterpret_cast<WCHAR*>(data));
    }
    RegCloseKey(h);
  }
  // ProgIds registered for this extension.
  std::vector<std::wstring> progids;
  for (HKEY base : {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE}) {
    std::wstring key =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + ext +
        L"\\OpenWithProgids";
    HKEY h = nullptr;
    if (RegOpenKeyExW(base, key.c_str(), 0, KEY_READ, &h) != ERROR_SUCCESS) continue;
    for (DWORD i = 0;; ++i) {
      WCHAR vn[256]{};
      DWORD vns = 256;
      if (RegEnumValueW(h, i, vn, &vns, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
        break;
      progids.push_back(vn);
    }
    RegCloseKey(h);
  }
  auto clsid_default = reg_default_string(HKEY_CLASSES_ROOT, ext);
  if (!clsid_default.empty()) progids.push_back(clsid_default);
  for (auto& pid : progids) {
    auto cmd = reg_default_string(HKEY_CLASSES_ROOT, pid + L"\\shell\\open\\command");
    if (cmd.empty()) continue;
    auto exe = exe_from_command(cmd);
    if (!exe.empty()) exes.push_back(std::move(exe));
  }
}

}  // namespace

std::vector<OpenWithApp> native_apps_for_file(const std::string& path, std::size_t max_apps) {
  std::vector<OpenWithApp> out;
  if (max_apps == 0) return out;
  auto ext = utf8_to_wide(path_extension(path));
  if (ext.empty() || ext[0] != L'.') return out;
  std::vector<std::wstring> cands;
  collect_openwith_candidates(ext, cands);
  std::vector<std::wstring> seen;
  for (auto& c : cands) {
    if (out.size() >= max_apps) break;
    std::wstring resolved = (c.find(L'\\') != std::wstring::npos || c.find(L'/') != std::wstring::npos)
                                ? (win_file_exists(c) ? c : std::wstring())
                                : resolve_exe_name(c);
    if (resolved.empty()) {
      // OpenWithList entries are bare exe names; command-derived ones are full paths.
      resolved = resolve_exe_name(c);
      if (resolved.empty()) continue;
    }
    auto low = win_lower(resolved);
    bool dup = false;
    for (auto& s : seen)
      if (s == low) {
        dup = true;
        break;
      }
    if (dup) continue;
    seen.push_back(low);
    OpenWithApp a;
    a.name = exe_display_name(resolved);
    a.target = wide_to_utf8(resolved);
    out.push_back(std::move(a));
  }
  return out;
}

bool native_open_with(const std::string& target, const std::string& file) {
  if (target.empty() || file.empty()) return false;
  auto app = utf8_to_wide(target);
  auto f = utf8_to_wide(file);
  std::wstring args = L"\"" + f + L"\"";
  std::wstring dir = utf8_to_wide(path_parent(file));
  auto rc = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", app.c_str(), args.c_str(),
                                                   dir.empty() ? nullptr : dir.c_str(),
                                                   SW_SHOWNORMAL));
  return rc > 32;
}

bool native_open_terminal(const std::string& dir) {
  if (dir.empty()) return false;
  auto wdir = utf8_to_wide(dir);
  WCHAR exe[MAX_PATH]{};
  if (SearchPathW(nullptr, L"wt.exe", nullptr, MAX_PATH, exe, nullptr)) {
    std::wstring args = L"-d \"" + wdir + L"\"";
    auto rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", exe, args.c_str(), wdir.c_str(), SW_SHOWNORMAL));
    if (rc > 32) return true;
  }
  if (SearchPathW(nullptr, L"powershell.exe", nullptr, MAX_PATH, exe, nullptr)) {
    std::wstring q = wdir;
    for (std::size_t p = 0; (p = q.find(L'\'', p)) != std::wstring::npos;) {
      q.replace(p, 1, L"''");
      p += 2;
    }
    std::wstring args = L"-NoExit -Command \"Set-Location -LiteralPath '" + q + L"'\"";
    auto rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", exe, args.c_str(), wdir.c_str(), SW_SHOWNORMAL));
    if (rc > 32) return true;
  }
  if (SearchPathW(nullptr, L"cmd.exe", nullptr, MAX_PATH, exe, nullptr)) {
    std::wstring args = L"/K cd /d \"" + wdir + L"\"";
    auto rc = reinterpret_cast<INT_PTR>(
        ShellExecuteW(nullptr, L"open", exe, args.c_str(), wdir.c_str(), SW_SHOWNORMAL));
    return rc > 32;
  }
  return false;
}

bool native_open_editor(const std::string& dir) {
  if (dir.empty()) return false;
  WCHAR exe[MAX_PATH]{};
  for (auto probe : {L"code.exe", L"codium.exe"}) {
    if (SearchPathW(nullptr, probe, nullptr, MAX_PATH, exe, nullptr)) {
      std::wstring args = L"--new-window \"" + utf8_to_wide(dir) + L"\"";
      auto rc = reinterpret_cast<INT_PTR>(
          ShellExecuteW(nullptr, L"open", exe, args.c_str(), nullptr, SW_SHOWNORMAL));
      if (rc > 32) return true;
    }
  }
  return native_launch(dir);
}

#else
bool native_launch(const std::string&);
#endif
}  // namespace wilfred
