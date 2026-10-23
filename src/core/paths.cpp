#include "wilfred/core/paths.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/platform.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
// windows.h first: shlobj.h needs its types.
#include <windows.h>

#include <shlobj.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <pwd.h>
#include <unistd.h>
#include <climits>
#include <cstdlib>
#else
#include <pwd.h>
#include <unistd.h>
#include <climits>
#if defined(__FreeBSD__) || defined(__DragonFly__) || defined(__NetBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#if defined(__OpenBSD__)
#include <dlfcn.h>
#endif
#endif

namespace wilfred {
namespace fs = std::filesystem;

// argv[0] saved at startup via set_program_invocation(). Set once before
// worker threads spawn; read-only afterwards.
static std::string& program_invocation_storage() {
  static std::string s;
  return s;
}

void set_program_invocation(const char* argv0) {
  if (argv0 && *argv0) program_invocation_storage() = argv0;
}

#if !defined(_WIN32) && !defined(__APPLE__)
namespace {
// Resolve the saved argv[0] to an absolute executable path: "a/b/prog" via
// realpath (cwd-relative), bare "prog" via PATH search. Returns "" when it
// cannot be resolved to an existing file.
std::string resolve_program_invocation() {
  const std::string& a = program_invocation_storage();
  if (a.empty()) return {};
  if (a.find('/') != std::string::npos) {
    char real[PATH_MAX];
    if (realpath(a.c_str(), real)) return std::string(real);
    return {};
  }
  const char* path_env = std::getenv("PATH");
  if (!path_env || !*path_env) return {};
  std::string dirs(path_env);
  std::string::size_type start = 0;
  while (start <= dirs.size()) {
    auto end = dirs.find(':', start);
    std::string dir =
        (end == std::string::npos) ? dirs.substr(start) : dirs.substr(start, end - start);
    if (dir.empty()) dir = ".";
    std::string cand = dir + "/" + a;
    if (access(cand.c_str(), X_OK) == 0) {
      char real[PATH_MAX];
      if (realpath(cand.c_str(), real)) return std::string(real);
      return cand;
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return {};
}
}  // namespace
#endif

#if defined(__OpenBSD__)
namespace {
// getexecpath(3) exists on OpenBSD 8.0+ (<unistd.h>). Look it up at runtime
// so the same binary still starts on 7.x, where the symbol does not exist.
std::string openbsd_getexecpath() {
  using getexecpath_fn = int (*)(char*, std::size_t);
  void* sym = dlsym(RTLD_DEFAULT, "getexecpath");
  if (!sym) return {};
  getexecpath_fn fn = nullptr;
  std::memcpy(&fn, &sym, sizeof(fn));
  char tmp[PATH_MAX];
  if (fn(tmp, sizeof(tmp)) != 0) return {};
  tmp[sizeof(tmp) - 1] = '\0';
  return std::string(tmp);
}
}  // namespace
#endif

char path_separator() {
#ifdef _WIN32
  return '\\';
#else
  return '/';
#endif
}

static std::string replace_slashes(std::string s) {
#ifdef _WIN32
  for (char& c : s)
    if (c == '/') c = '\\';
#else
  for (char& c : s)
    if (c == '\\') c = '/';
#endif
  return s;
}

std::string path_join(std::string_view a, std::string_view b) {
  if (a.empty()) return std::string(b);
  if (b.empty()) return std::string(a);
  std::string out(a);
  char sep = path_separator();
  if (out.back() != '/' && out.back() != '\\') out.push_back(sep);
  if (b.front() == '/' || b.front() == '\\')
    out.append(b.substr(1));
  else
    out.append(b);
  return replace_slashes(out);
}

std::string path_normalize(std::string_view p) {
  if (p.empty()) return {};
  std::error_code ec;
  fs::path fp = fs::u8path(std::string(p));
  auto n = fs::weakly_canonical(fp, ec);
  if (ec) n = fp.lexically_normal();
  auto u8 = n.u8string();
  return replace_slashes(std::string(u8.begin(), u8.end()));
}

std::string path_filename(std::string_view p) {
  auto s = std::string(p);
  auto pos = s.find_last_of("/\\");
  if (pos == std::string::npos) return s;
  return s.substr(pos + 1);
}

std::string path_stem(std::string_view p) {
  auto name = path_filename(p);
  auto dot = name.find_last_of('.');
  if (dot == std::string::npos || dot == 0) return name;
  return name.substr(0, dot);
}

std::string path_extension(std::string_view p) {
  auto name = path_filename(p);
  auto dot = name.find_last_of('.');
  if (dot == std::string::npos || dot == 0) return {};
  return to_lower_utf8(name.substr(dot));
}

std::string path_parent(std::string_view p) {
  auto s = std::string(p);
  auto pos = s.find_last_of("/\\");
  if (pos == std::string::npos) return {};
  if (pos == 0) return s.substr(0, 1);
#ifdef _WIN32
  if (pos == 2 && s.size() > 1 && s[1] == ':') return s.substr(0, 3);
#endif
  return s.substr(0, pos);
}

std::string path_lower(std::string_view p) {
#ifdef _WIN32
  return to_lower_utf8(p);
#else
  return std::string(p);
#endif
}

bool path_is_absolute(std::string_view p) {
  if (p.empty()) return false;
#ifdef _WIN32
  if (p.size() >= 2 && ((p[1] == ':') || (p[0] == '\\' && p[1] == '\\'))) return true;
#endif
  return p[0] == '/' || p[0] == '\\';
}

static std::string env_or(const char* key, const std::string& fallback) {
#ifdef _WIN32
  wchar_t buf[32767];
  auto wkey = utf8_to_wide(key);
  DWORD n = GetEnvironmentVariableW(wkey.c_str(), buf, 32767);
  if (n == 0 || n >= 32767) return fallback;
  return wide_to_utf8(std::wstring(buf, n));
#else
  const char* v = std::getenv(key);
  return v && *v ? std::string(v) : fallback;
#endif
}

std::string home_directory() {
#ifdef _WIN32
  PWSTR w = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Profile, 0, nullptr, &w)) && w) {
    std::string s = wide_to_utf8(w);
    CoTaskMemFree(w);
    return s;
  }
  return env_or("USERPROFILE", "C:\\Users\\Default");
#else
  const char* h = std::getenv("HOME");
  if (h && *h) return h;
  if (auto* pw = getpwuid(getuid())) return pw->pw_dir;
  return "/";
#endif
}

std::string config_directory() {
#ifdef _WIN32
  PWSTR w = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &w)) && w) {
    std::string s = path_join(wide_to_utf8(w), "Wilfred");
    CoTaskMemFree(w);
    return s;
  }
  return path_join(home_directory(), "AppData/Roaming/Wilfred");
#elif defined(__APPLE__)
  return path_join(home_directory(), "Library/Application Support/Wilfred");
#else
  auto xdg = env_or("XDG_CONFIG_HOME", "");
  if (!xdg.empty()) return path_join(xdg, "wilfred");
  return path_join(home_directory(), ".config/wilfred");
#endif
}

std::string data_directory() {
#ifdef _WIN32
  PWSTR w = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &w)) && w) {
    std::string s = path_join(wide_to_utf8(w), "Wilfred");
    CoTaskMemFree(w);
    return s;
  }
  return path_join(home_directory(), "AppData/Local/Wilfred");
#elif defined(__APPLE__)
  return path_join(home_directory(), "Library/Application Support/Wilfred");
#else
  auto xdg = env_or("XDG_DATA_HOME", "");
  if (!xdg.empty()) return path_join(xdg, "wilfred");
  return path_join(home_directory(), ".local/share/wilfred");
#endif
}

std::string default_config_path() {
  return path_join(config_directory(), "wilfred.yml");
}
std::string default_index_path() {
  return path_join(data_directory(), "index");
}
std::string default_history_path() {
  return path_join(data_directory(), "history.bin");
}
std::string default_clips_path() {
  return path_join(data_directory(), "clips.bin");
}
std::string default_log_path() {
  return path_join(data_directory(), "wilfred.log");
}
std::string default_onboard_path() {
  return path_join(data_directory(), "onboard");
}

bool is_onboarding_done() {
  std::error_code ec;
  return std::filesystem::exists(std::filesystem::path(default_onboard_path()), ec) && !ec;
}

void mark_onboarding_done() {
  std::error_code ec;
  std::filesystem::create_directories(std::filesystem::path(default_onboard_path()).parent_path(), ec);
  std::ofstream f(default_onboard_path(), std::ios::trunc);
  // Empty file is enough; its existence is the marker.
}

std::string exe_directory() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (!n) return ".";
  return path_parent(wide_to_utf8(std::wstring(buf, n)));
#elif defined(__APPLE__)
  char buf[1024];
  uint32_t sz = sizeof(buf);
  if (_NSGetExecutablePath(buf, &sz) != 0) return ".";
  char real[PATH_MAX];
  if (!realpath(buf, real)) return path_parent(buf);
  return path_parent(real);
#else
  char buf[PATH_MAX];
#if defined(__FreeBSD__) || defined(__DragonFly__)
  {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    std::size_t len = sizeof(buf);
    if (sysctl(mib, 4, buf, &len, nullptr, 0) == 0 && len > 1) {
      buf[sizeof(buf) - 1] = '\0';
      return path_parent(buf);
    }
  }
#elif defined(__NetBSD__)
  {
    int mib[4] = {CTL_KERN, KERN_PROC_ARGS, static_cast<int>(getpid()), KERN_PROC_PATHNAME};
    std::size_t len = sizeof(buf);
    if (sysctl(mib, 4, buf, &len, nullptr, 0) == 0 && len > 1) {
      buf[sizeof(buf) - 1] = '\0';
      return path_parent(buf);
    }
  }
#elif defined(__OpenBSD__)
  // OpenBSD has no sysctl for the executable path and procfs is usually
  // unmounted: prefer getexecpath(3) on 8.0+, else fall through to
  // procfs/argv[0] below.
  {
    std::string p = openbsd_getexecpath();
    if (!p.empty()) return path_parent(p);
  }
#endif
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) n = readlink("/proc/curproc/exe", buf, sizeof(buf) - 1);
  if (n <= 0) {
    // Last resort: argv[0] saved at startup (covers OpenBSD without procfs).
    std::string p = resolve_program_invocation();
    if (!p.empty()) return path_parent(p);
    return ".";
  }
  buf[n] = 0;
  return path_parent(buf);
#endif
}

std::string exe_path() {
#ifdef _WIN32
  wchar_t buf[MAX_PATH];
  DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (!n) return "";
  return wide_to_utf8(std::wstring(buf, n));
#elif defined(__APPLE__)
  char buf[1024];
  uint32_t sz = sizeof(buf);
  if (_NSGetExecutablePath(buf, &sz) != 0) return "";
  char real[PATH_MAX];
  if (!realpath(buf, real)) return std::string(buf);
  return std::string(real);
#else
  char buf[PATH_MAX];
#if defined(__FreeBSD__) || defined(__DragonFly__)
  {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
    std::size_t len = sizeof(buf);
    if (sysctl(mib, 4, buf, &len, nullptr, 0) == 0 && len > 1) {
      buf[sizeof(buf) - 1] = '\0';
      return std::string(buf);
    }
  }
#elif defined(__NetBSD__)
  {
    int mib[4] = {CTL_KERN, KERN_PROC_ARGS, static_cast<int>(getpid()), KERN_PROC_PATHNAME};
    std::size_t len = sizeof(buf);
    if (sysctl(mib, 4, buf, &len, nullptr, 0) == 0 && len > 1) {
      buf[sizeof(buf) - 1] = '\0';
      return std::string(buf);
    }
  }
#elif defined(__OpenBSD__)
  // OpenBSD has no sysctl for the executable path and procfs is usually
  // unmounted: prefer getexecpath(3) on 8.0+, else fall through to
  // procfs/argv[0] below.
  {
    std::string p = openbsd_getexecpath();
    if (!p.empty()) return p;
  }
#endif
  ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
  if (n <= 0) n = readlink("/proc/curproc/exe", buf, sizeof(buf) - 1);
  if (n <= 0) {
    // Last resort: argv[0] saved at startup (covers OpenBSD without procfs).
    std::string p = resolve_program_invocation();
    if (!p.empty()) return p;
    return "";
  }
  buf[n] = 0;
  return std::string(buf);
#endif
}

std::string ipc_endpoint() {
#ifdef _WIN32
  return "\\\\.\\pipe\\wilfred";
#else
  return path_join(data_directory(), "wilfred.sock");
#endif
}

std::vector<std::string> default_index_roots() {
  std::vector<std::string> roots;
  roots.push_back(home_directory());
#ifdef _WIN32
  roots.push_back("C:\\Program Files");
  roots.push_back("C:\\Program Files (x86)");
  PWSTR w = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_StartMenu, 0, nullptr, &w)) && w) {
    roots.push_back(wide_to_utf8(w));
    CoTaskMemFree(w);
  }
  w = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_CommonStartMenu, 0, nullptr, &w)) && w) {
    roots.push_back(wide_to_utf8(w));
    CoTaskMemFree(w);
  }
#elif defined(WILFRED_IOS)
  // Sandboxed: only the app container (pushed above) is visible. There is
  // no /Applications, no shared storage and no app-enumeration API, so the
  // Swift layer registers nothing and the index covers app files only.
#elif defined(__APPLE__)
  roots.push_back("/Applications");
  roots.push_back(path_join(home_directory(), "Applications"));
  roots.push_back("/usr/local");
  roots.push_back("/opt");
#elif defined(__ANDROID__)
  // App-private files are always present; shared storage needs
  // READ_EXTERNAL_STORAGE / media permissions granted in the UI.
  roots.push_back("/sdcard");
  roots.push_back(path_join(home_directory(), "files"));
#else
  roots.push_back("/usr/share/applications");
  roots.push_back("/usr/local");
  roots.push_back("/opt");
  roots.push_back(path_join(home_directory(), ".local/share/applications"));
#endif
  return roots;
}

std::vector<std::string> default_system_directories() {
#ifdef _WIN32
  return {"C:\\Windows", "C:\\Windows\\System32", "C:\\Windows\\SysWOW64",
          "C:\\ProgramData\\Microsoft"};
#elif defined(WILFRED_IOS)
  return {};
#elif defined(__APPLE__)
  return {"/System", "/private/var", "/Library/Apple"};
#elif defined(__ANDROID__)
  return {"/proc", "/sys", "/dev", "/system", "/vendor"};
#else
  return {"/proc", "/sys", "/dev", "/run", "/boot"};
#endif
}

bool path_equals(std::string_view a, std::string_view b) {
#ifdef _WIN32
  return to_lower_utf8(replace_slashes(std::string(a))) ==
         to_lower_utf8(replace_slashes(std::string(b)));
#else
  return a == b;
#endif
}

bool path_is_under(std::string_view path, std::string_view root) {
  auto p = path_normalize(path);
  auto r = path_normalize(root);
#ifdef _WIN32
  auto pl = to_lower_utf8(p);
  auto rl = to_lower_utf8(r);
  if (pl == rl) return true;
  if (pl.size() < rl.size()) return false;
  if (pl.compare(0, rl.size(), rl) != 0) return false;
  return pl.size() == rl.size() || pl[rl.size()] == '\\' || pl[rl.size()] == '/';
#else
  if (p == r) return true;
  if (p.size() < r.size()) return false;
  if (p.compare(0, r.size(), r) != 0) return false;
  return p.size() == r.size() || p[r.size()] == '/';
#endif
}

std::string native_path(std::string_view utf8) {
  return replace_slashes(std::string(utf8));
}

}  // namespace wilfred
