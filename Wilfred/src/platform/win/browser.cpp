#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#endif

namespace wilfred {
#ifdef _WIN32

static std::string reg_sz(HKEY root, const wchar_t* path, const wchar_t* name) {
  HKEY k{};
  if (RegOpenKeyExW(root, path, 0, KEY_READ, &k) != ERROR_SUCCESS) return {};
  wchar_t buf[1024];
  DWORD t = 0, sz = sizeof(buf);
  std::string out;
  if (RegQueryValueExW(k, name, nullptr, &t, reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS)
    out = wide_to_utf8(buf);
  RegCloseKey(k);
  return out;
}

std::string native_default_browser_id() {
  auto prog = reg_sz(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice",
                     L"ProgId");
  auto l = to_lower_utf8(prog);
  if (l.find("chrome") != std::string::npos) return "chrome";
  if (l.find("firefox") != std::string::npos) return "firefox";
  if (l.find("edge") != std::string::npos) return "edge";
  if (l.find("brave") != std::string::npos) return "brave";
  if (l.find("opera") != std::string::npos) return "opera";
  if (!prog.empty()) return prog;
  return "default";
}

std::string native_default_browser_executable() {
  auto prog = reg_sz(HKEY_CURRENT_USER,
                     L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice",
                     L"ProgId");
  if (prog.empty()) prog = "http";
  std::wstring cmdkey = utf8_to_wide(prog) + L"\\shell\\open\\command";
  auto cmd = reg_sz(HKEY_CLASSES_ROOT, cmdkey.c_str(), nullptr);
  if (cmd.size() >= 2 && cmd.front() == '"') {
    auto end = cmd.find('"', 1);
    if (end != std::string::npos) return cmd.substr(1, end - 1);
  }
  auto sp = cmd.find(' ');
  return sp == std::string::npos ? cmd : cmd.substr(0, sp);
}

std::vector<BrowserInfo> native_list_browsers() {
  std::vector<BrowserInfo> out;
  struct Cand {
    const wchar_t* id;
    const wchar_t* name;
    const wchar_t* path;
  };
  Cand cands[] = {
      {L"chrome", L"Google Chrome",
       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\chrome.exe"},
      {L"firefox", L"Mozilla Firefox",
       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\firefox.exe"},
      {L"msedge", L"Microsoft Edge",
       L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\msedge.exe"},
      {L"brave", L"Brave", L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\brave.exe"},
  };
  auto def = native_default_browser_executable();
  auto defl = to_lower_utf8(def);
  for (auto& c : cands) {
    auto exe = reg_sz(HKEY_LOCAL_MACHINE, c.path, nullptr);
    if (exe.empty()) continue;
    BrowserInfo b;
    b.id = wide_to_utf8(c.id);
    b.name = wide_to_utf8(c.name);
    b.executable = exe;
    b.is_default = to_lower_utf8(exe) == defl || defl.find(to_lower_utf8(b.id)) != std::string::npos;
    out.push_back(std::move(b));
  }
  if (out.empty()) {
    BrowserInfo b;
    b.id = native_default_browser_id();
    b.name = b.id;
    b.executable = def;
    b.is_default = true;
    out.push_back(b);
  }
  return out;
}

#else
std::string native_default_browser_id();
#endif
}  // namespace wilfred
