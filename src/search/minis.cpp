#include "wilfred/search/minis.hpp"

#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/volumes.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/macros.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winhttp.h>
#else
#include <dirent.h>
#include <fstream>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#endif

namespace wilfred {
namespace {

bool g_net = true;
std::mutex weather_mu;
std::string weather_cache;
std::int64_t weather_cache_at{0};

static SearchResult card(const std::string& title, const std::string& sub, const std::string& payload,
                         const std::string& label, int score = 10000,
                         ResultAction action = ResultAction::Copy, int meter = -1) {
  SearchResult r;
  r.title = title;
  r.subtitle = sub;
  r.payload = payload.empty() ? title : payload;
  r.path = r.payload;
  r.action = action;
  r.score = score;
  r.kind_label = label;
  r.category = "mini";
  r.meter = meter;
  return r;
}

static std::string human_bytes(std::uint64_t n) {
  const char* u[] = {"B", "KB", "MB", "GB", "TB", "PB"};
  double v = static_cast<double>(n);
  int i = 0;
  while (v >= 1024.0 && i < 5) {
    v /= 1024.0;
    ++i;
  }
  char buf[64];
  if (i == 0)
    std::snprintf(buf, sizeof(buf), "%llu %s", static_cast<unsigned long long>(n), u[i]);
  else
    std::snprintf(buf, sizeof(buf), "%.1f %s", v, u[i]);
  return buf;
}

static std::string trim_sv(std::string s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
  return s;
}

static std::string format_uptime(std::uint64_t sec) {
  auto d = sec / 86400;
  auto h = (sec % 86400) / 3600;
  auto m = (sec % 3600) / 60;
  std::ostringstream os;
  if (d) os << d << "d ";
  os << h << "h " << m << "m";
  return os.str();
}

#ifdef _WIN32
static std::string http_get_https(const wchar_t* host, const std::wstring& path) {
  std::string body;
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return body;
  WinHttpSetTimeouts(ses, 1200, 1200, 1200, 1800);
  HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return body;
  }
  HINTERNET req = WinHttpOpenRequest(con, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return body;
  }
  BOOL ok = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (ok) ok = WinHttpReceiveResponse(req, nullptr);
  if (ok) {
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(req, &avail) && avail) {
      std::string chunk(avail, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(req, chunk.data(), avail, &read)) break;
      chunk.resize(read);
      body += chunk;
      if (body.size() > 4096) break;
    }
  }
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return body;
}
#else
static std::string http_get_https(const char* url) {
  std::string cmd = std::string("curl -fsS --max-time 2 -A Wilfred/1.0 \"") + url + "\" 2>/dev/null";
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) return {};
  std::string body;
  char buf[512];
  while (fgets(buf, sizeof(buf), f)) {
    body += buf;
    if (body.size() > 4096) break;
  }
  pclose(f);
  return body;
}
#endif

static std::string fetch_weather(const std::string& where) {
  if (!g_net) return {};
  auto now = unix_seconds();
  std::string key = to_lower_utf8(where);
  {
    std::lock_guard<std::mutex> lock(weather_mu);
    if (weather_cache_at && now - weather_cache_at < 600 && weather_cache.find(key) == 0)
      return weather_cache.substr(key.size() + 1);
  }
  std::string loc = where.empty() ? "" : percent_encode(where);
#ifdef _WIN32
  std::wstring path = L"/";
  if (!loc.empty()) {
    path += utf8_to_wide(loc);
  }
  path += L"?format=%l:+%c+%t+%h+%w+%C";
  auto body = http_get_https(L"wttr.in", path);
#else
  std::string url = "https://wttr.in/";
  if (!loc.empty()) url += loc;
  url += "?format=%l:+%c+%t+%h+%w+%C";
  auto body = http_get_https(url.c_str());
#endif
  while (!body.empty() && (body.back() == '\n' || body.back() == '\r')) body.pop_back();
  if (body.empty() || body.find("Unknown") != std::string::npos) return {};
  std::lock_guard<std::mutex> lock(weather_mu);
  weather_cache = key + "|" + body;
  weather_cache_at = now;
  return body;
}

struct DriveUse {
  std::string path;
  std::string name;
  std::uint64_t total{0};
  std::uint64_t free{0};
};

static std::vector<DriveUse> drive_usage() {
  std::vector<DriveUse> out;
#ifdef _WIN32
  for (auto& v : list_volumes()) {
    if (!v.ready) continue;
    ULARGE_INTEGER free_b{}, total_b{}, dummy{};
    auto w = utf8_to_wide(v.path);
    if (!GetDiskFreeSpaceExW(w.c_str(), &dummy, &total_b, &free_b)) continue;
    DriveUse d;
    d.path = v.path;
    d.name = v.name.empty() ? v.path : v.name;
    d.total = total_b.QuadPart;
    d.free = free_b.QuadPart;
    out.push_back(std::move(d));
  }
#else
  std::vector<std::string> roots = {"/"};
  for (auto& v : list_volumes())
    if (!v.path.empty()) roots.push_back(v.path);
  std::sort(roots.begin(), roots.end());
  roots.erase(std::unique(roots.begin(), roots.end()), roots.end());
  for (auto& p : roots) {
    struct statvfs st {};
    if (statvfs(p.c_str(), &st) != 0) continue;
    DriveUse d;
    d.path = p;
    d.name = p;
    d.total = static_cast<std::uint64_t>(st.f_blocks) * st.f_frsize;
    d.free = static_cast<std::uint64_t>(st.f_bavail) * st.f_frsize;
    if (d.total == 0) continue;
    out.push_back(std::move(d));
  }
#endif
  return out;
}

static void ram_stats(std::uint64_t& total, std::uint64_t& used) {
  total = 0;
  used = 0;
#ifdef _WIN32
  MEMORYSTATUSEX ms{};
  ms.dwLength = sizeof(ms);
  if (GlobalMemoryStatusEx(&ms)) {
    total = ms.ullTotalPhys;
    used = ms.ullTotalPhys - ms.ullAvailPhys;
  }
#elif defined(__APPLE__)
  std::uint64_t mem = 0;
  std::size_t sz = sizeof(mem);
  sysctlbyname("hw.memsize", &mem, &sz, nullptr, 0);
  total = mem;
  used = 0;
#else
  std::ifstream in("/proc/meminfo");
  std::string k;
  std::uint64_t kb = 0;
  std::string unit;
  std::uint64_t avail = 0;
  while (in >> k >> kb >> unit) {
    if (k == "MemTotal:") total = kb * 1024;
    if (k == "MemAvailable:") avail = kb * 1024;
  }
  if (total) used = total - avail;
#endif
}

static double cpu_percent() {
#ifdef _WIN32
  static FILETIME prev_idle{}, prev_kernel{}, prev_user{};
  static bool have = false;
  FILETIME idle{}, kernel{}, user{};
  if (!GetSystemTimes(&idle, &kernel, &user)) return -1;
  auto qw = [](const FILETIME& f) {
    return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime;
  };
  if (!have) {
    prev_idle = idle;
    prev_kernel = kernel;
    prev_user = user;
    have = true;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    if (!GetSystemTimes(&idle, &kernel, &user)) return -1;
  }
  auto didle = qw(idle) - qw(prev_idle);
  auto dker = qw(kernel) - qw(prev_kernel);
  auto dusr = qw(user) - qw(prev_user);
  prev_idle = idle;
  prev_kernel = kernel;
  prev_user = user;
  auto tot = dker + dusr;
  if (!tot) return 0;
  double busy = static_cast<double>(tot - didle) / static_cast<double>(tot);
  return std::clamp(busy * 100.0, 0.0, 100.0);
#else
  static unsigned long long prev_idle = 0, prev_total = 0;
  std::ifstream in("/proc/stat");
  std::string cpu;
  unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, soft = 0,
                     steal = 0;
  if (!(in >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> soft >> steal)) return -1;
  auto idle_all = idle + iowait;
  auto total = user + nice + system + idle_all + irq + soft + steal;
  if (!prev_total) {
    prev_idle = idle_all;
    prev_total = total;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    in.clear();
    in.seekg(0);
    if (!(in >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> soft >> steal)) return -1;
    idle_all = idle + iowait;
    total = user + nice + system + idle_all + irq + soft + steal;
  }
  auto didle = idle_all - prev_idle;
  auto dtot = total - prev_total;
  prev_idle = idle_all;
  prev_total = total;
  if (!dtot) return 0;
  return std::clamp(100.0 * (1.0 - static_cast<double>(didle) / static_cast<double>(dtot)), 0.0,
                    100.0);
#endif
}

struct ProcInfo {
  std::uint32_t pid{0};
  std::string name;
  std::uint64_t working_set{0};
  std::uint32_t threads{0};
  double cpu{-1};
};

static std::vector<ProcInfo> list_processes(const std::string& needle) {
  std::vector<ProcInfo> out;
  auto want = to_lower_utf8(needle);
#ifdef _WIN32
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return out;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(snap, &pe)) {
    do {
      auto name = wide_to_utf8(pe.szExeFile);
      auto folded = to_lower_utf8(name);
      if (!want.empty() && folded.find(want) == std::string::npos) continue;
      ProcInfo p;
      p.pid = pe.th32ProcessID;
      p.name = name;
      p.threads = pe.cntThreads;
      HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION |
                                 PROCESS_VM_READ,
                             FALSE, pe.th32ProcessID);
      if (h) {
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
          p.working_set = pmc.WorkingSetSize;
        FILETIME c{}, e{}, k{}, u{};
        if (GetProcessTimes(h, &c, &e, &k, &u)) {
          auto qw = [](FILETIME f) {
            return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime;
          };
          SYSTEM_INFO si{};
          GetSystemInfo(&si);
          auto cpu_time = qw(k) + qw(u);
          FILETIME now_ft{};
          GetSystemTimeAsFileTime(&now_ft);
          auto wall = qw(now_ft) - qw(c);
          if (wall > 0 && si.dwNumberOfProcessors)
            p.cpu = std::clamp(100.0 * static_cast<double>(cpu_time) / static_cast<double>(wall) /
                                   si.dwNumberOfProcessors,
                               0.0, 100.0);
        }
        CloseHandle(h);
      }
      out.push_back(std::move(p));
    } while (Process32NextW(snap, &pe));
  }
  CloseHandle(snap);
#else
  DIR* dir = opendir("/proc");
  if (!dir) return out;
  while (dirent* e = readdir(dir)) {
    char* end = nullptr;
    long pid = std::strtol(e->d_name, &end, 10);
    if (!end || *end || pid <= 0) continue;
    std::string base = std::string("/proc/") + e->d_name;
    std::ifstream comm(base + "/comm");
    std::string name;
    std::getline(comm, name);
    if (name.empty()) continue;
    auto folded = to_lower_utf8(name);
    if (!want.empty() && folded.find(want) == std::string::npos) continue;
    ProcInfo p;
    p.pid = static_cast<std::uint32_t>(pid);
    p.name = name;
    std::ifstream st(base + "/status");
    std::string line;
    while (std::getline(st, line)) {
      if (line.rfind("VmRSS:", 0) == 0) {
        unsigned long kb = 0;
        std::sscanf(line.c_str() + 6, "%lu", &kb);
        p.working_set = kb * 1024ull;
      } else if (line.rfind("Threads:", 0) == 0) {
        unsigned th = 0;
        std::sscanf(line.c_str() + 8, "%u", &th);
        p.threads = th;
      }
    }
    std::ifstream statf(base + "/stat");
    std::string statline;
    if (std::getline(statf, statline)) {
      auto rpar = statline.rfind(')');
      if (rpar != std::string::npos) {
        std::istringstream iss(statline.substr(rpar + 1));
        std::string state;
        unsigned long ppid = 0, pgrp = 0, sess = 0, tty = 0, tpgid = 0, flags = 0;
        unsigned long minflt = 0, cminflt = 0, majflt = 0, cmajflt = 0, utime = 0, stime = 0;
        iss >> state >> ppid >> pgrp >> sess >> tty >> tpgid >> flags >> minflt >> cminflt >>
            majflt >> cmajflt >> utime >> stime;
        (void)ppid;
        (void)pgrp;
        (void)sess;
        (void)tty;
        (void)tpgid;
        (void)flags;
        (void)minflt;
        (void)cminflt;
        (void)majflt;
        (void)cmajflt;
        long hz = sysconf(_SC_CLK_TCK);
        if (hz > 0) {
          double cpu_sec = static_cast<double>(utime + stime) / static_cast<double>(hz);
          std::ifstream up("/proc/uptime");
          double uptime = 1;
          up >> uptime;
          std::ifstream st2(base + "/stat");
          std::string dummy;
          long long start = 0;
          // starttime is field 22; we already parsed through stime (14 after comm)
          iss >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> start;
          (void)dummy;
          if (uptime > 0.2) p.cpu = std::clamp(100.0 * cpu_sec / uptime, 0.0, 100.0);
        }
      }
    }
    out.push_back(std::move(p));
  }
  closedir(dir);
#endif
  std::sort(out.begin(), out.end(), [](auto& a, auto& b) { return a.working_set > b.working_set; });
  if (out.size() > 8) out.resize(8);
  return out;
}

#ifdef _WIN32
static std::string first_ipv4() {
  WSADATA wsa{};
  WSAStartup(MAKEWORD(2, 2), &wsa);
  ULONG sz = 0;
  GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                    GAA_FLAG_SKIP_DNS_SERVER,
                       nullptr, nullptr, &sz);
  if (!sz) return {};
  std::vector<char> buf(sz);
  auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
  if (GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                                        GAA_FLAG_SKIP_DNS_SERVER,
                           nullptr, addrs, &sz) != NO_ERROR)
    return {};
  for (auto* a = addrs; a; a = a->Next) {
    if (a->OperStatus != IfOperStatusUp) continue;
    for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
      if (u->Address.lpSockaddr->sa_family != AF_INET) continue;
      char ip[64]{};
      getnameinfo(u->Address.lpSockaddr, (socklen_t)u->Address.iSockaddrLength, ip, sizeof(ip),
                  nullptr, 0, NI_NUMERICHOST);
      if (std::strcmp(ip, "127.0.0.1") == 0) continue;
      return ip;
    }
  }
  return {};
}
#else
static std::string first_ipv4() {
  FILE* f = popen("hostname -I 2>/dev/null", "r");
  if (!f) return {};
  char buf[128]{};
  if (!fgets(buf, sizeof(buf), f)) {
    pclose(f);
    return {};
  }
  pclose(f);
  std::string s(buf);
  auto sp = s.find(' ');
  if (sp != std::string::npos) s.resize(sp);
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
  return s;
}
#endif

}  // namespace

void set_mini_network_enabled(bool enabled) { g_net = enabled; }

MiniIntent parse_mini_intent(std::string_view query) {
  MiniIntent it;
  std::string s = trim_sv(std::string(query));
  if (s.empty()) return it;
  auto l = to_lower_utf8(s);
  auto space = l.find(' ');
  std::string key = space == std::string::npos ? l : l.substr(0, space);
  std::string rest = space == std::string::npos ? std::string() : trim_sv(s.substr(space + 1));
  auto colon = key.find(':');
  if (colon != std::string::npos) {
    rest = trim_sv(s.substr(colon + 1));
    key = key.substr(0, colon);
  }
  auto set = [&](MiniKind k, bool exact) {
    it.kind = k;
    it.remainder = rest;
    it.exact = exact;
  };
  if (key == "weather" || key == "wttr" || key == "forecast") set(MiniKind::Weather, rest.empty());
  else if (key == "time" || key == "date" || key == "clock" || key == "now")
    set(MiniKind::Time, true);
  else if (key == "disku" || key == "diskusage" || key == "disk-usage")
    set(MiniKind::DiskUsage, true);
  else if (key == "disk" || key == "disks" || key == "storage" || key == "drives")
    set(MiniKind::Disk, true);
  else if (key == "ram" || key == "memory" || key == "mem")
    set(MiniKind::Ram, true);
  else if (key == "cpu" || key == "processor")
    set(MiniKind::Cpu, true);
  else if (key == "process" || key == "proc" || key == "ps" || key == "top" || key == "processes")
    set(MiniKind::Process, rest.empty());
  else if (key == "battery" || key == "power")
    set(MiniKind::Battery, true);
  else if (key == "hostname" || key == "host")
    set(MiniKind::Hostname, true);
  else if (key == "ip" || key == "ipaddress")
    set(MiniKind::Ip, true);
  else if (key == "uptime")
    set(MiniKind::Uptime, true);
  else if (key == "user" || key == "whoami")
    set(MiniKind::User, true);
  else if (key == "clip" || key == "clipboard")
    set(MiniKind::Clipboard, true);
  else if (key == "clips" || key == "cliphist" || key == "pasteboard")
    set(MiniKind::Clips, true);
  else if (key == "os" || key == "systeminfo" || key == "sysinfo")
    set(MiniKind::Os, true);
  else if (key == "cores" || key == "nproc" || key == "threads")
    set(MiniKind::Cores, true);
  else if (key == "screen" || key == "resolution" || key == "display")
    set(MiniKind::Screen, true);
  else if (key == "swap" || key == "pagefile" || key == "vmem")
    set(MiniKind::Swap, true);
  else if (key == "help" || key == "minis" || key == "cmds" || key == "commands")
    set(MiniKind::Help, true);
  else if (key == "macros" || key == "bangs")
    set(MiniKind::MacrosList, true);
  return it;
}

std::vector<SearchResult> mini_results(const std::string& query, const Config& cfg,
                                       const std::string& clipboard) {
  std::vector<SearchResult> out;
  if (!cfg.search.minis) return out;
  auto intent = parse_mini_intent(query);
  if (intent.kind == MiniKind::None) return out;

  if (intent.kind == MiniKind::Time) {
    auto t = static_cast<std::time_t>(unix_seconds());
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char clock[64];
    char date[64];
    std::strftime(clock, sizeof(clock), "%I:%M:%S %p", &local);
    std::strftime(date, sizeof(date), "%A, %B %d, %Y", &local);
    auto payload = std::string(clock) + " · " + date;
    out.push_back(card(clock, date, payload, "time"));
    return out;
  }

  if (intent.kind == MiniKind::Weather) {
    auto w = fetch_weather(intent.remainder);
    if (!w.empty()) {
      out.push_back(card(w, intent.remainder.empty() ? "Local weather · enter copies"
                                                     : "Weather · " + intent.remainder,
                         w, "weather"));
    } else {
      std::string url = "https://wttr.in/";
      if (!intent.remainder.empty()) url += percent_encode(intent.remainder);
      SearchResult r = card("Weather", "Open live forecast", url, "weather", 9900,
                            ResultAction::WebSearch);
      r.path = url;
      out.push_back(std::move(r));
    }
    return out;
  }

  if (intent.kind == MiniKind::Ram) {
    std::uint64_t total = 0, used = 0;
    ram_stats(total, used);
    if (total) {
      int pct = static_cast<int>((used * 100) / total);
      char title[128];
      std::snprintf(title, sizeof(title), "RAM  %d%%  ·  %s used of %s", pct,
                    human_bytes(used).c_str(), human_bytes(total).c_str());
      out.push_back(card(title, "Physical memory", title, "ram", 10000, ResultAction::Copy, pct));
    }
    return out;
  }

  if (intent.kind == MiniKind::Cpu) {
    auto pct = cpu_percent();
    char title[64];
    if (pct < 0)
      std::snprintf(title, sizeof(title), "CPU");
    else
      std::snprintf(title, sizeof(title), "CPU  %.0f%%", pct);
    out.push_back(card(title, "Processor load", title, "cpu", 10000, ResultAction::Copy,
                      pct < 0 ? -1 : static_cast<int>(pct)));
    return out;
  }

  if (intent.kind == MiniKind::Disk || intent.kind == MiniKind::DiskUsage) {
    auto drives = drive_usage();
    std::uint64_t ttot = 0, tfree = 0;
    for (auto& d : drives) {
      ttot += d.total;
      tfree += d.free;
      auto used = d.total > d.free ? d.total - d.free : 0;
      int pct = d.total ? static_cast<int>((used * 100) / d.total) : 0;
      char line[192];
      std::snprintf(line, sizeof(line), "%s  %d%% used  ·  %s free of %s", d.path.c_str(), pct,
                    human_bytes(d.free).c_str(), human_bytes(d.total).c_str());
      auto r = card(line, d.name, line, intent.kind == MiniKind::DiskUsage ? "disku" : "disk", 10000,
                    ResultAction::Copy, pct);
#ifdef _WIN32
      r.action = ResultAction::Open;
      r.path = d.path;
      r.payload = d.path;
#endif
      out.push_back(std::move(r));
    }
    if (intent.kind == MiniKind::DiskUsage && ttot) {
      auto used = ttot - tfree;
      int pct = static_cast<int>((used * 100) / ttot);
      char sum[160];
      std::snprintf(sum, sizeof(sum), "All drives  %d%% used  ·  %s of %s", pct,
                    human_bytes(used).c_str(), human_bytes(ttot).c_str());
      out.insert(out.begin(), card(sum, "Total disk usage", sum, "disku", 10050, ResultAction::Copy, pct));
    }
    return out;
  }

  if (intent.kind == MiniKind::Process) {
    auto procs = list_processes(intent.remainder);
    if (procs.empty()) {
      out.push_back(card("No matching process",
                         intent.remainder.empty() ? "Type process <name>" : intent.remainder, "",
                         "process", 9000, ResultAction::None));
      return out;
    }
    for (auto& p : procs) {
      char title[192];
      if (p.cpu >= 0)
        std::snprintf(title, sizeof(title), "%s  ·  CPU %.1f%%  ·  RAM %s", p.name.c_str(), p.cpu,
                      human_bytes(p.working_set).c_str());
      else
        std::snprintf(title, sizeof(title), "%s  ·  RAM %s", p.name.c_str(),
                      human_bytes(p.working_set).c_str());
      char sub[128];
      std::snprintf(sub, sizeof(sub), "PID %u  ·  %u threads", p.pid, p.threads);
      int meter = p.cpu >= 0 ? static_cast<int>(p.cpu) : -1;
      out.push_back(card(title, sub, title, "process", 10000, ResultAction::Copy, meter));
    }
    return out;
  }

  if (intent.kind == MiniKind::Battery) {
#ifdef _WIN32
    SYSTEM_POWER_STATUS st{};
    if (GetSystemPowerStatus(&st)) {
      if (st.BatteryFlag == 128) {
        out.push_back(card("No battery", "AC powered desktop", "No battery", "battery"));
      } else {
        char title[80];
        const char* src = st.ACLineStatus == 1 ? "plugged in" : "on battery";
        std::snprintf(title, sizeof(title), "Battery  %d%%  ·  %s", (int)st.BatteryLifePercent, src);
        out.push_back(card(title, "Power", title, "battery", 10000, ResultAction::Copy,
                           st.BatteryLifePercent <= 100 ? (int)st.BatteryLifePercent : -1));
      }
    }
#else
    int pct = -1;
    std::string status = "unknown";
    {
      std::ifstream cap("/sys/class/power_supply/BAT0/capacity");
      if (!(cap >> pct)) {
        std::ifstream cap1("/sys/class/power_supply/BAT1/capacity");
        cap1 >> pct;
      }
      std::ifstream st("/sys/class/power_supply/BAT0/status");
      if (!(st >> status)) {
        std::ifstream st1("/sys/class/power_supply/BAT1/status");
        st1 >> status;
      }
    }
    if (pct >= 0) {
      char title[80];
      std::snprintf(title, sizeof(title), "Battery  %d%%  ·  %s", pct, status.c_str());
      out.push_back(card(title, "Power", title, "battery", 10000, ResultAction::Copy, pct));
    } else {
      out.push_back(card("Battery", "Unavailable on this platform", "", "battery", 8000,
                         ResultAction::None));
    }
#endif
    return out;
  }

  if (intent.kind == MiniKind::Hostname) {
    char name[256]{};
#ifdef _WIN32
    DWORD n = 256;
    GetComputerNameA(name, &n);
#else
    gethostname(name, sizeof(name) - 1);
#endif
    out.push_back(card(name, "Hostname", name, "host"));
    return out;
  }

  if (intent.kind == MiniKind::Ip) {
    auto ip = first_ipv4();
    if (ip.empty()) ip = "127.0.0.1";
    out.push_back(card(ip, "IPv4 address", ip, "ip"));
    return out;
  }

  if (intent.kind == MiniKind::Uptime) {
#ifdef _WIN32
    auto sec = GetTickCount64() / 1000;
#else
    std::uint64_t sec = 0;
    std::ifstream up("/proc/uptime");
    double u = 0;
    if (up >> u) sec = static_cast<std::uint64_t>(u);
#endif
    auto t = format_uptime(sec);
    out.push_back(card("Uptime  " + t, "Since last boot", t, "uptime"));
    return out;
  }

  if (intent.kind == MiniKind::User) {
#ifdef _WIN32
    char user[256]{};
    DWORD n = 256;
    GetUserNameA(user, &n);
    out.push_back(card(user, "Signed-in user", user, "user"));
#else
    const char* u = getenv("USER");
    out.push_back(card(u ? u : "user", "Signed-in user", u ? u : "", "user"));
#endif
    return out;
  }

  if (intent.kind == MiniKind::Clipboard) {
    if (clipboard.empty()) {
      out.push_back(card("Clipboard is empty", "Copy something, then search clip", "", "clipboard",
                         9000, ResultAction::None));
    } else {
      auto show = clipboard;
      if (show.size() > 90) show = show.substr(0, 87) + "...";
      out.push_back(card(show, "Clipboard · enter copies", clipboard, "clipboard"));
    }
    return out;
  }

  if (intent.kind == MiniKind::Clips) {
    auto hist = clipboard_history_texts();
    if (hist.empty()) {
      out.push_back(card("No clipboard history yet", "Copy text, then type clips", "", "clips", 9000,
                         ResultAction::None));
    } else {
      int n = 0;
      for (auto& t : hist) {
        out.push_back(card(clipboard_preview(t), "Clipboard history", t, "clips", 10000 - n));
        if (++n >= 8) break;
      }
    }
    return out;
  }

  if (intent.kind == MiniKind::Os) {
#if defined(_WIN32)
    const char* os = "Windows";
#elif defined(__APPLE__)
    const char* os = "macOS";
#else
    const char* os = "Linux";
#endif
    std::string bits = sizeof(void*) == 8 ? "64-bit" : "32-bit";
    auto title = std::string(os) + "  ·  " + bits;
    out.push_back(card(title, "Operating system", title, "os"));
    return out;
  }

  if (intent.kind == MiniKind::Cores) {
    unsigned n = 0;
#ifdef _WIN32
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    n = si.dwNumberOfProcessors;
#else
    long v = sysconf(_SC_NPROCESSORS_ONLN);
    n = v > 0 ? static_cast<unsigned>(v) : 0;
#endif
    char title[64];
    std::snprintf(title, sizeof(title), "%u logical processors", n);
    out.push_back(card(title, "CPU cores", title, "cores"));
    return out;
  }

  if (intent.kind == MiniKind::Screen) {
#ifdef _WIN32
    int w = GetSystemMetrics(SM_CXSCREEN);
    int h = GetSystemMetrics(SM_CYSCREEN);
    char title[64];
    std::snprintf(title, sizeof(title), "%d × %d", w, h);
    out.push_back(card(title, "Primary display", title, "screen"));
#else
    out.push_back(card("Display", "Resolution unavailable here", "", "screen", 8000,
                       ResultAction::None));
#endif
    return out;
  }

  if (intent.kind == MiniKind::Swap) {
#ifdef _WIN32
    MEMORYSTATUSEX ms{};
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
      auto total = ms.ullTotalPageFile;
      auto used = total > ms.ullAvailPageFile ? total - ms.ullAvailPageFile : 0;
      int pct = total ? static_cast<int>((used * 100) / total) : 0;
      char title[128];
      std::snprintf(title, sizeof(title), "Page file  %d%%  ·  %s of %s", pct,
                    human_bytes(used).c_str(), human_bytes(total).c_str());
      out.push_back(card(title, "Commit charge", title, "swap", 10000, ResultAction::Copy, pct));
    }
#else
    std::ifstream in("/proc/meminfo");
    std::string k, unit;
    std::uint64_t kb = 0, total = 0, freeb = 0;
    while (in >> k >> kb >> unit) {
      if (k == "SwapTotal:") total = kb * 1024;
      if (k == "SwapFree:") freeb = kb * 1024;
    }
    if (total) {
      auto used = total - freeb;
      int pct = static_cast<int>((used * 100) / total);
      char title[128];
      std::snprintf(title, sizeof(title), "Swap  %d%%  ·  %s of %s", pct, human_bytes(used).c_str(),
                    human_bytes(total).c_str());
      out.push_back(card(title, "Swap space", title, "swap", 10000, ResultAction::Copy, pct));
    } else {
      out.push_back(card("No swap", "Swap is unused or disabled", "No swap", "swap"));
    }
#endif
    return out;
  }

  if (intent.kind == MiniKind::Help) {
    static const char* lines[] = {
        "weather [city]  ·  local forecast",
        "time  ·  clock and date",
        "disk / disku  ·  drive space",
        "ram / cpu / swap  ·  memory and load",
        "process <name>  ·  app CPU and RAM",
        "battery / ip / hostname / uptime / user",
        "clip / clips  ·  clipboard",
        "os / cores / screen",
        "macros  ·  bang searches  (!yt cats)",
        nullptr};
    for (auto** p = lines; *p; ++p)
      out.push_back(card(*p, "Mini commands · enter copies", *p, "help", 10000));
    return out;
  }

  if (intent.kind == MiniKind::MacrosList) {
    auto macros = merged_macros(cfg);
    std::vector<std::string> names;
    names.reserve(macros.size());
    for (auto& [name, tmpl] : macros) {
      (void)tmpl;
      names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    int n = 0;
    for (auto& name : names) {
      SearchResult r;
      r.title = "!" + name;
      r.subtitle = macros[name];
      r.payload = "!" + name + " ";
      r.path = r.payload;
      r.action = ResultAction::Habit;
      r.score = 9800 - n;
      r.kind_label = "macro";
      r.category = "mini";
      out.push_back(std::move(r));
      if (++n >= 24) break;
    }
    return out;
  }

  return out;
}

}  // namespace wilfred
