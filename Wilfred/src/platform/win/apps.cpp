#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#include <filesystem>

namespace wilfred {
#ifdef _WIN32
namespace fs = std::filesystem;

static bool parse_lnk(const std::string& lnk, std::string& target) {
  auto w = utf8_to_wide(lnk);
  IShellLinkW* sl = nullptr;
  HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void**>(&sl));
  if (FAILED(hr) || !sl) return false;
  IPersistFile* pf = nullptr;
  hr = sl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&pf));
  if (FAILED(hr) || !pf) {
    sl->Release();
    return false;
  }
  hr = pf->Load(w.c_str(), STGM_READ);
  wchar_t path[MAX_PATH];
  WIN32_FIND_DATAW fd{};
  bool ok = SUCCEEDED(hr) && SUCCEEDED(sl->GetPath(path, MAX_PATH, &fd, SLGP_UNCPRIORITY));
  if (ok) target = wide_to_utf8(path);
  pf->Release();
  sl->Release();
  return ok && !target.empty();
}

static void scan_dir_apps(const std::string& dir, std::vector<AppInfo>& out, bool system) {
  std::error_code ec;
  fs::recursive_directory_iterator it(fs::u8path(dir),
                                      fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    auto ext = it->path().extension().wstring();
    auto u8p = it->path().u8string();
    std::string path(u8p.begin(), u8p.end());
    auto nameu = it->path().stem().u8string();
    std::string name(nameu.begin(), nameu.end());
    if (_wcsicmp(ext.c_str(), L".lnk") == 0) {
      std::string tgt;
      if (parse_lnk(path, tgt)) {
        AppInfo a;
        a.name = name;
        a.path = tgt.empty() ? path : tgt;
        a.keywords.push_back(name);
        a.system = system;
        out.push_back(std::move(a));
      }
    } else if (_wcsicmp(ext.c_str(), L".exe") == 0) {
      AppInfo a;
      a.name = name;
      a.path = path;
      a.system = system;
      out.push_back(std::move(a));
    }
  }
}

static std::string known(REFKNOWNFOLDERID id) {
  PWSTR w = nullptr;
  if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &w)) || !w) return {};
  std::string s = wide_to_utf8(w);
  CoTaskMemFree(w);
  return s;
}

std::vector<AppInfo> native_discover_apps() {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  std::vector<AppInfo> apps;
  auto sm = known(FOLDERID_StartMenu);
  auto csm = known(FOLDERID_CommonStartMenu);
  auto desk = known(FOLDERID_Desktop);
  if (!sm.empty()) scan_dir_apps(path_join(sm, "Programs"), apps, false);
  if (!csm.empty()) scan_dir_apps(path_join(csm, "Programs"), apps, true);
  if (!desk.empty()) scan_dir_apps(desk, apps, false);
  // App Paths registry
  HKEY k{};
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths", 0, KEY_READ,
                    &k) == ERROR_SUCCESS) {
    wchar_t name[256];
    for (DWORD i = 0;; ++i) {
      DWORD nlen = 256;
      if (RegEnumKeyExW(k, i, name, &nlen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS)
        break;
      HKEY sub{};
      if (RegOpenKeyExW(k, name, 0, KEY_READ, &sub) != ERROR_SUCCESS) continue;
      wchar_t val[MAX_PATH];
      DWORD t = 0, sz = sizeof(val);
      if (RegQueryValueExW(sub, nullptr, nullptr, &t, reinterpret_cast<LPBYTE>(val), &sz) ==
          ERROR_SUCCESS) {
        AppInfo a;
        a.name = wide_to_utf8(std::wstring(name));
        if (a.name.size() > 4 && to_lower_utf8(a.name).substr(a.name.size() - 4) == ".exe")
          a.name = a.name.substr(0, a.name.size() - 4);
        a.path = wide_to_utf8(val);
        apps.push_back(std::move(a));
      }
      RegCloseKey(sub);
    }
    RegCloseKey(k);
  }
  return apps;
}
#else
std::vector<AppInfo> native_discover_apps();
#endif
}  // namespace wilfred
