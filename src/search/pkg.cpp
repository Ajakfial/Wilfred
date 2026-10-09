#include "wilfred/search/pkg.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/search/engine.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace wilfred {
namespace {

// Canonical manager order (also the `packages` mini order).
const char* kManagers[] = {"winget", "brew", "apt", "choco", "flatpak", "pacman", nullptr};

const char* kManagerExe(const std::string& mgr) {
  // Every supported manager's binary matches its id.
  for (auto** p = kManagers; *p; ++p)
    if (mgr == *p) return *p;
  return nullptr;
}

std::string trim_copy(const std::string& s) {
  std::string o = s;
  while (!o.empty() &&
         (o.front() == ' ' || o.front() == '\t' || o.front() == '\r' || o.front() == '\n'))
    o.erase(o.begin());
  while (!o.empty() &&
         (o.back() == ' ' || o.back() == '\t' || o.back() == '\r' || o.back() == '\n'))
    o.pop_back();
  return o;
}

bool have_tool(const std::string& exe) {
#ifdef _WIN32
  return std::system(("where " + exe + " >NUL 2>&1").c_str()) == 0;
#elif defined(WILFRED_IOS)
  return false;  // no subprocesses and no package managers on iOS
#else
  return std::system(("command -v " + exe + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

// Split on runs of 2+ spaces/tabs (console table columns).
std::vector<std::string> split_columns(const std::string& line) {
  std::vector<std::string> out;
  std::string cur;
  std::size_t blanks = 0;
  auto flush = [&] {
    auto t = trim_copy(cur);
    if (!t.empty()) out.push_back(t);
    cur.clear();
  };
  for (char c : line) {
    if (c == ' ' || c == '\t') {
      ++blanks;
      if (blanks >= 2) {
        flush();
        blanks = 0;
      } else {
        cur.push_back(c);
      }
    } else {
      blanks = 0;
      cur.push_back(c);
    }
  }
  flush();
  return out;
}

bool valid_token(const std::string& s) {
  if (s.empty() || s.size() > 256) return false;
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-' || c == '+' || c == '@' || c == '/' || c == ':';
    if (!ok) return false;
  }
  return true;
}

bool valid_apt_name(const std::string& s) {
  if (s.empty() || s.size() > 128) return false;
  for (char c : s) {
    bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '+' ||
              c == '-' || c == ':';
    if (!ok) return false;
  }
  return true;
}

// Run argv with a hard timeout; stdout captured up to max_bytes (stderr
// discarded so warnings never pollute parsers). Returns exit code,
// -1 on spawn failure, -2 on timeout (child killed).
int pkg_run_capture(const std::vector<std::string>& argv, int timeout_ms, std::size_t max_bytes,
                    std::string& out) {
  out.clear();
  if (argv.empty() || timeout_ms < 1000) return -1;
#ifdef _WIN32
  std::string cmd;
  for (auto& a : argv) {
    if (!cmd.empty()) cmd.push_back(' ');
    bool quote = a.empty() || a.find_first_of(" \t\"") != std::string::npos;
    if (!quote) {
      cmd += a;
      continue;
    }
    cmd.push_back('"');
    for (char c : a) {
      if (c == '"') cmd.push_back('\\');
      cmd.push_back(c);
    }
    cmd.push_back('"');
  }
  SECURITY_ATTRIBUTES sa{};
  sa.nLength = sizeof(sa);
  sa.bInheritHandle = TRUE;
  HANDLE out_r = nullptr, out_w = nullptr;
  if (!CreatePipe(&out_r, &out_w, &sa, 0)) return -1;
  SetHandleInformation(out_r, HANDLE_FLAG_INHERIT, 0);
  HANDLE nul = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa,
                           OPEN_EXISTING, 0, nullptr);
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdInput = nul ? nul : nullptr;
  si.hStdOutput = out_w;
  si.hStdError = out_w;
  PROCESS_INFORMATION pi{};
  auto wcmd = utf8_to_wide(cmd);
  BOOL ok = CreateProcessW(nullptr, wcmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                           nullptr, &si, &pi);
  CloseHandle(out_w);
  if (nul) CloseHandle(nul);
  if (!ok) {
    CloseHandle(out_r);
    return -1;
  }
  auto deadline = GetTickCount64() + static_cast<ULONGLONG>(timeout_ms);
  char buf[4096];
  DWORD exit_code = 0;
  bool timed_out = false;
  for (;;) {
    DWORD avail = 0;
    if (PeekNamedPipe(out_r, nullptr, 0, nullptr, &avail, nullptr) && avail) {
      DWORD n = 0;
      if (!ReadFile(out_r, buf, sizeof(buf), &n, nullptr) || n == 0) break;
      if (out.size() < max_bytes) out.append(buf, std::min<std::size_t>(n, max_bytes - out.size()));
    } else {
      DWORD st = WaitForSingleObject(pi.hProcess, 10);
      if (st == WAIT_OBJECT_0) break;
      if (GetTickCount64() > deadline) {
        TerminateProcess(pi.hProcess, 1);
        timed_out = true;
        break;
      }
    }
  }
  // Drain anything left after exit.
  for (;;) {
    DWORD avail = 0;
    if (!PeekNamedPipe(out_r, nullptr, 0, nullptr, &avail, nullptr) || !avail) break;
    DWORD n = 0;
    if (!ReadFile(out_r, buf, sizeof(buf), &n, nullptr) || n == 0) break;
    if (out.size() < max_bytes) out.append(buf, std::min<std::size_t>(n, max_bytes - out.size()));
  }
  if (!timed_out && !GetExitCodeProcess(pi.hProcess, &exit_code)) exit_code = 1;
  CloseHandle(out_r);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return timed_out ? -2 : static_cast<int>(exit_code);
#else
  int out_pipe[2];
  if (pipe(out_pipe) != 0) return -1;
  pid_t pid = fork();
  if (pid < 0) {
    close(out_pipe[0]);
    close(out_pipe[1]);
    return -1;
  }
  if (pid == 0) {
    dup2(out_pipe[1], STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    close(out_pipe[0]);
    close(out_pipe[1]);
    std::vector<std::string> store = argv;
    std::vector<char*> av;
    for (auto& a : store)
      av.push_back(a.data());
    av.push_back(nullptr);
    execvp(av[0], av.data());
    _exit(127);
  }
  close(out_pipe[1]);
  int flags = fcntl(out_pipe[0], F_GETFL, 0);
  fcntl(out_pipe[0], F_SETFL, flags | O_NONBLOCK);
  char buf[4096];
  auto start = std::chrono::steady_clock::now();
  bool timed_out = false;
  int status = 0;
  bool reaped = false;
  for (;;) {
    auto got = read(out_pipe[0], buf, sizeof(buf));
    if (got > 0) {
      if (out.size() < max_bytes)
        out.append(buf,
                   std::min<std::size_t>(static_cast<std::size_t>(got), max_bytes - out.size()));
    } else {
      pid_t w = waitpid(pid, &status, WNOHANG);
      if (w == pid) {
        reaped = true;
        break;
      }
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start)
                    .count();
      if (ms > timeout_ms) {
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
        reaped = true;
        timed_out = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }
  }
  // Drain remainder after exit.
  for (;;) {
    auto got = read(out_pipe[0], buf, sizeof(buf));
    if (got <= 0) break;
    if (out.size() < max_bytes)
      out.append(buf, std::min<std::size_t>(static_cast<std::size_t>(got), max_bytes - out.size()));
  }
  close(out_pipe[0]);
  if (!reaped) {
    int st = 0;
    waitpid(pid, &st, WNOHANG);
  }
  if (timed_out) return -2;
  return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
#endif
}

std::vector<std::string> allowed_managers(const Config& cfg) {
  std::vector<std::string> all;
  for (auto** p = kManagers; *p; ++p)
    all.push_back(*p);
  if (cfg.packages.managers.empty()) {
#ifdef _WIN32
    return {"winget", "choco"};
#elif defined(__APPLE__)
    return {"brew"};
#elif defined(__ANDROID__) || defined(WILFRED_IOS)
    return {};
#else
    return {"apt", "flatpak", "pacman", "brew"};
#endif
  }
  std::vector<std::string> out;
  for (auto& m : all) {
    for (auto& want : cfg.packages.managers)
      if (to_lower_utf8(want) == m) {
        out.push_back(m);
        break;
      }
  }
  return out;
}

}  // namespace

bool pkg_is_manager(const std::string& word) {
  auto l = to_lower_utf8(trim_copy(word));
  for (auto** p = kManagers; *p; ++p)
    if (l == *p) return true;
  return false;
}

std::vector<std::string> pkg_known_managers() {
  std::vector<std::string> out;
  for (auto** p = kManagers; *p; ++p)
    out.push_back(*p);
  return out;
}

PkgIntent pkg_parse_intent(const std::string& text) {
  PkgIntent it;
  auto t = trim_copy(text);
  if (t.empty()) return it;
  auto sp = t.find_first_of(" \t");
  std::string first = sp == std::string::npos ? t : t.substr(0, sp);
  std::string rest = sp == std::string::npos ? std::string() : trim_copy(t.substr(sp + 1));
  auto low = to_lower_utf8(first);
  if (low == "pkg") {
    // `pkg <manager> <query>`.
    auto sp2 = rest.find_first_of(" \t");
    if (sp2 == std::string::npos) return it;
    auto mgr = to_lower_utf8(rest.substr(0, sp2));
    auto q = trim_copy(rest.substr(sp2 + 1));
    if (!pkg_is_manager(mgr) || q.empty()) return it;
    it.matched = true;
    it.manager = mgr;
    it.query = q;
    return it;
  }
  if (!pkg_is_manager(low) || rest.empty()) return it;
  it.matched = true;
  it.manager = low;
  it.query = rest;
  return it;
}

std::vector<PkgHit> pkg_parse_winget(const std::string& out) {
  std::vector<PkgHit> hits;
  bool in_table = false;
  std::istringstream is(out);
  std::string line;
  while (std::getline(is, line)) {
    auto t = trim_copy(line);
    if (t.empty()) continue;
    auto low = to_lower_utf8(t);
    if (!in_table && low.rfind("name", 0) == 0 && low.find("id") != std::string::npos) {
      in_table = true;
      continue;
    }
    if (!t.empty() && t[0] == '-') continue;
    auto f = split_columns(t);
    if (f.size() < 3) {
      // Locale fallback: accept rows whose middle field looks like an id.
      if (in_table || f.size() < 2) continue;
      if (f.size() == 2 && f[1].find('.') != std::string::npos) {
        hits.push_back({f[0], f[1], "", ""});
        if (hits.size() >= 50) break;
      }
      continue;
    }
    in_table = true;
    if (!valid_token(f[1])) continue;
    PkgHit h;
    h.name = f[0];
    h.id = f[1];
    h.version = f[2];
    h.source = f.size() > 3 ? f[3] : "";
    hits.push_back(std::move(h));
    if (hits.size() >= 50) break;
  }
  return hits;
}

std::vector<PkgHit> pkg_parse_brew(const std::string& out) {
  std::vector<PkgHit> hits;
  std::istringstream is(out);
  std::string line;
  while (std::getline(is, line)) {
    auto t = trim_copy(line);
    if (t.empty() || t.rfind("==>", 0) == 0) continue;
    if (!valid_token(t) || t.find(' ') != std::string::npos) continue;
    hits.push_back({t, t, "", ""});
    if (hits.size() >= 50) break;
  }
  return hits;
}

std::vector<PkgHit> pkg_parse_apt(const std::string& out) {
  std::vector<PkgHit> hits;
  std::istringstream is(out);
  std::string line;
  while (std::getline(is, line)) {
    if (line.empty() || line[0] == ' ' || line[0] == '\t') continue;
    auto t = trim_copy(line);
    auto slash = t.find('/');
    if (slash == std::string::npos || slash == 0) continue;
    std::string name = t.substr(0, slash);
    std::string rest = trim_copy(t.substr(slash + 1));
    auto sp = rest.find_first_of(" \t");
    std::string ver = sp == std::string::npos ? rest : rest.substr(0, sp);
    if (!valid_apt_name(name) || ver.empty() || ver.size() > 64) continue;
    hits.push_back({name, name, ver, ""});
    if (hits.size() >= 50) break;
  }
  return hits;
}

std::vector<PkgHit> pkg_parse_choco(const std::string& out) {
  std::vector<PkgHit> hits;
  std::istringstream is(out);
  std::string line;
  while (std::getline(is, line)) {
    auto t = trim_copy(line);
    if (t.empty()) continue;
    auto bar = t.find('|');
    if (bar == std::string::npos || bar == 0) continue;
    std::string id = t.substr(0, bar);
    std::string ver = trim_copy(t.substr(bar + 1));
    auto pipe2 = ver.find('|');
    if (pipe2 != std::string::npos) ver.resize(pipe2);
    if (!valid_token(id) || id.find(' ') != std::string::npos) continue;
    hits.push_back({id, id, ver, ""});
    if (hits.size() >= 50) break;
  }
  return hits;
}

std::vector<PkgHit> pkg_parse_flatpak(const std::string& out) {
  std::vector<PkgHit> hits;
  std::istringstream is(out);
  std::string line;
  bool header_seen = false;
  while (std::getline(is, line)) {
    auto t = trim_copy(line);
    if (t.empty()) continue;
    auto low = to_lower_utf8(t);
    if (!header_seen && low.find("application") != std::string::npos) {
      header_seen = true;
      continue;
    }
    auto f = split_columns(t);
    if (f.size() < 3) continue;
    // Columns: Name, Description, Application ID, Version, Branch, Remotes.
    if (f[2].find('.') == std::string::npos || !valid_token(f[2])) continue;
    header_seen = true;
    PkgHit h;
    h.name = f[0];
    h.id = f[2];
    h.version = f.size() > 3 ? f[3] : "";
    std::string remotes = f.size() > 5 ? f[5] : (f.size() > 4 ? f[4] : "");
    auto comma = remotes.find(',');
    h.source =
        comma == std::string::npos ? trim_copy(remotes) : trim_copy(remotes.substr(0, comma));
    hits.push_back(std::move(h));
    if (hits.size() >= 50) break;
  }
  return hits;
}

std::vector<PkgHit> pkg_parse_pacman(const std::string& out) {
  std::vector<PkgHit> hits;
  std::istringstream is(out);
  std::string line;
  while (std::getline(is, line)) {
    if (line.empty() || line[0] == ' ' || line[0] == '\t') continue;
    auto t = trim_copy(line);
    auto slash = t.find('/');
    if (slash == std::string::npos || slash == 0) continue;
    std::string rest = trim_copy(t.substr(slash + 1));
    auto sp = rest.find_first_of(" \t");
    if (sp == std::string::npos) continue;
    std::string name = rest.substr(0, sp);
    std::string ver = trim_copy(rest.substr(sp + 1));
    auto sp2 = ver.find(' ');
    if (sp2 != std::string::npos) ver.resize(sp2);
    if (!valid_token(name) || ver.empty()) continue;
    hits.push_back({name, name, ver, t.substr(0, slash)});
    if (hits.size() >= 50) break;
  }
  return hits;
}

std::string pkg_install_command(const std::string& manager, const std::string& id,
                                const std::string& source) {
  if (manager == "winget") return "winget install --exact --id " + id;
  if (manager == "brew") return "brew install " + id;
  if (manager == "apt") return "sudo apt install -y " + id;
  if (manager == "choco") return "choco install " + id + " -y";
  if (manager == "flatpak")
    return "flatpak install -y " + (source.empty() ? "" : source + " ") + id;
  if (manager == "pacman") return "sudo pacman -S --noconfirm " + id;
  return manager + " install " + id;
}

namespace {

// Fast PATH probes with a 60s TTL so per-keystroke provider queries stay cheap.
std::vector<std::string> detected_cached(const std::vector<std::string>& candidates) {
  static std::mutex mu;
  static std::map<std::string, std::pair<bool, std::chrono::steady_clock::time_point>> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto now = std::chrono::steady_clock::now();
  std::vector<std::string> out;
  for (auto& m : candidates) {
    const char* exe = kManagerExe(m);
    if (!exe) continue;
    auto it = cache.find(exe);
    if (it == cache.end() ||
        std::chrono::duration_cast<std::chrono::seconds>(now - it->second.second).count() > 60) {
      cache[exe] = {have_tool(exe), now};
    }
    if (cache[exe].first) out.push_back(m);
  }
  return out;
}

std::vector<PkgHit> search_with(const std::string& mgr, const std::string& q, int timeout_ms) {
  std::string body;
  if (mgr == "brew") {
    // Formulae first, then casks; dedupe by id.
    std::string f, c;
    pkg_run_capture({"brew", "search", "--formula", q}, timeout_ms, 64 * 1024, f);
    pkg_run_capture({"brew", "search", "--cask", q}, timeout_ms, 64 * 1024, c);
    auto hits = pkg_parse_brew(f);
    auto seen = [&](const std::string& id) {
      for (auto& h : hits)
        if (h.id == id) return true;
      return false;
    };
    for (auto& h : pkg_parse_brew(c))
      if (!seen(h.id)) hits.push_back(h);
    return hits;
  }
  std::vector<std::string> argv;
  if (mgr == "winget")
    argv = {"winget", "search", q, "--disable-interactivity", "--accept-source-agreements"};
  else if (mgr == "apt")
    argv = {"apt", "search", q};
  else if (mgr == "choco")
    argv = {"choco", "search", q, "-r"};
  else if (mgr == "flatpak")
    argv = {"flatpak", "search", q};
  else if (mgr == "pacman")
    argv = {"pacman", "-Ss", q};
  else
    return {};
  pkg_run_capture(argv, timeout_ms, 64 * 1024, body);
  if (mgr == "winget") return pkg_parse_winget(body);
  if (mgr == "apt") return pkg_parse_apt(body);
  if (mgr == "choco") return pkg_parse_choco(body);
  if (mgr == "flatpak") return pkg_parse_flatpak(body);
  return pkg_parse_pacman(body);
}

}  // namespace

std::vector<std::string> pkg_detected_managers(const Config& cfg) {
  return detected_cached(allowed_managers(cfg));
}

std::vector<SearchResult> pkg_managers_results(const std::string& remainder, const Config& cfg) {
  // A full provider invocation (`pkg <manager> <query>`) belongs to the
  // provider, not this list — stay out of its way.
  if (pkg_parse_intent(std::string("pkg ") + remainder).matched) return {};
  std::vector<SearchResult> out;
  auto filter = to_lower_utf8(trim_copy(remainder));
  auto detected = pkg_detected_managers(cfg);
  auto is_detected = [&](const std::string& m) {
    for (auto& d : detected)
      if (d == m) return true;
    return false;
  };
  const char* hint[] = {"install via the Microsoft Store / App Installer",
                        "install from brew.sh",
                        "built into Debian/Ubuntu",
                        "install from chocolatey.org",
                        "needs a configured remote (e.g. flathub)",
                        "built into Arch",
                        nullptr};
  int i = 0;
  for (auto** p = kManagers; *p; ++p, ++i) {
    std::string mgr(*p);
    if (!filter.empty() && mgr.find(filter) == std::string::npos) continue;
    bool allowed = false;
    for (auto& a : allowed_managers(cfg))
      if (a == mgr) allowed = true;
    if (!allowed) continue;
    bool found = is_detected(mgr);
    SearchResult r;
    r.title = mgr + (found ? " — available" : " — not on PATH");
    r.subtitle =
        found ? (std::string("e.g. `") + mgr + " firefox` — Enter copies an example") : hint[i];
    r.path = "";
    r.payload = mgr + " firefox";
    r.action = ResultAction::Mini;
    r.score = 9000;
    r.kind_label = "pkg";
    r.category = "pkg";
    out.push_back(std::move(r));
  }
  if (out.empty()) {
    SearchResult r;
    r.title = "No package managers match";
    r.subtitle = "packages — Enter copies the list";
    r.payload = "packages";
    r.action = ResultAction::Mini;
    r.score = 9000;
    r.kind_label = "pkg";
    r.category = "pkg";
    out.push_back(std::move(r));
  }
  return out;
}

bool pkg_install(const std::string& manager, const std::string& id, std::string& error) {
  error.clear();
  if (!pkg_is_manager(manager)) {
    error = "unknown package manager '" + manager + "'";
    return false;
  }
  if (id.empty() || id.size() > 256) {
    error = "invalid package id";
    return false;
  }
  std::vector<std::string> argv;
  if (manager == "winget") {
    argv = {"winget",
            "install",
            "--exact",
            "--id",
            id,
            "--silent",
            "--accept-package-agreements",
            "--accept-source-agreements",
            "--disable-interactivity"};
  } else if (manager == "brew") {
    argv = {"brew", "install", id};
  } else if (manager == "apt") {
    if (!have_tool("pkexec")) {
      error = "needs a terminal: run `sudo apt install -y " + id + "`";
      return false;
    }
    argv = {"pkexec", "apt", "install", "-y", id};
  } else if (manager == "choco") {
    argv = {"choco", "install", id, "-y"};
  } else if (manager == "flatpak") {
    // Encoded as <remote>:<app-id> by the provider.
    auto colon = id.find(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= id.size()) {
      error = "flatpak needs a remote: install from the Flathub website or `flatpak install <app>`";
      return false;
    }
    argv = {"flatpak", "install", "-y", id.substr(0, colon), id.substr(colon + 1)};
  } else if (manager == "pacman") {
    if (!have_tool("pkexec")) {
      error = "needs a terminal: run `sudo pacman -S " + id + "`";
      return false;
    }
    argv = {"pkexec", "pacman", "-S", "--noconfirm", id};
  }
  std::string body;
  int rc = pkg_run_capture(argv, 600000, 256 * 1024, body);
  if (rc == -2) {
    error = "install timed out after 10 minutes";
    return false;
  }
  if (rc != 0) {
    error = "install failed (exit " + std::to_string(rc) + ")";
    return false;
  }
  return true;
}

std::vector<SearchResult> PkgProvider::query(const std::string& text, const Config& cfg,
                                             std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.packages.enabled || limit == 0) return out;
  auto intent = pkg_parse_intent(text);
  if (!intent.matched) return out;
  if (trim_copy(intent.query).size() < 2) return out;
  bool allowed = false;
  for (auto& a : allowed_managers(cfg))
    if (a == intent.manager) allowed = true;
  if (!allowed) return out;
  if (!have_tool(intent.manager)) return out;
  int budget = cfg.packages.max_results;
  if (budget < 1) budget = 1;
  if (budget > 20) budget = 20;
  auto hits = search_with(intent.manager, intent.query, cfg.packages.timeout_ms);
  int score = 5000;
  for (auto& h : hits) {
    if (out.size() >= static_cast<std::size_t>(budget) || out.size() >= limit) break;
    SearchResult r;
    r.title = h.name.empty() ? h.id : h.name;
    r.subtitle = h.id;
    if (!h.version.empty()) r.subtitle += " · " + h.version;
    r.subtitle += " · via " + intent.manager + " — Enter installs";
    std::string enc_id = h.id;
    if (intent.manager == "flatpak" && !h.source.empty()) enc_id = h.source + ":" + h.id;
    r.path = "pkg-install:" + intent.manager + ":" + enc_id;
    r.payload = pkg_install_command(intent.manager, h.id, h.source);
    r.action = ResultAction::Mini;
    r.score = score;
    if (score > 1000) score -= 10;
    r.kind_label = "pkg";
    r.category = "pkg";
    r.plugin_id = "";
    out.push_back(std::move(r));
  }
  return out;
}

}  // namespace wilfred
