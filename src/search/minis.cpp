#include "wilfred/search/minis.hpp"

#include "wilfred/browser/library.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/volumes.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/math/expr.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/clip_history.hpp"
#include "wilfred/search/disktools.hpp"
#include "wilfred/search/fuzzy.hpp"
#include "wilfred/search/glyphs.hpp"
#include "wilfred/search/layouts.hpp"
#include "wilfred/search/macros.hpp"
#include "wilfred/search/media.hpp"
#include "wilfred/search/nettools.hpp"
#include "wilfred/search/quicknotes.hpp"
#include "wilfred/search/screenshot.hpp"
#include "wilfred/search/timers.hpp"
#include "wilfred/search/transcribe.hpp"
#include "wilfred/search/workflows.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
// Order matters: winsock2.h and windows.h must precede iphlpapi.h and friends.
// clang-format off
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <winhttp.h>
// clang-format on
#else
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <fstream>
#if defined(__APPLE__) || defined(WILFRED_BSD)
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/user.h>
#endif
#if defined(__OpenBSD__) || defined(__NetBSD__)
// <sys/param.h> for DEV_BSIZE + MAXCOMLEN; <sys/swap.h> for swapctl(2).
#include <sys/param.h>
#include <sys/swap.h>
#endif
#if defined(__OpenBSD__)
// struct uvmexp via vm.uvmexp (same layout htop's OpenBSD backend uses).
#include <uvm/uvmexp.h>
#endif
#if defined(__NetBSD__)
// struct uvmexp via vm.uvmexp; kinfo_proc2 via <sys/sysctl.h> above.
#include <uvm/uvm_extern.h>
#endif
#if defined(WILFRED_BSD)
// Order matters: socket types precede interface and address headers.
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#endif
#endif

#if defined(__OpenBSD__)
// OpenBSD libc exposes only raw sysctl(2): neither sysctlbyname nor
// sysctlnametomib exist there. Map the names used in this TU to numeric
// MIBs so every call site below stays unchanged.
static int openbsd_sysctlbyname(const char* name, void* oldp, std::size_t* oldlenp, void* newp,
                                std::size_t newlen) {
  int mib[2] = {0, 0};
  if (std::strcmp(name, "kern.boottime") == 0) {
    mib[0] = CTL_KERN;
    mib[1] = KERN_BOOTTIME;
  } else if (std::strcmp(name, "hw.physmem") == 0) {
    mib[0] = CTL_HW;
    mib[1] = HW_PHYSMEM;
  } else if (std::strcmp(name, "kern.cp_time") == 0) {
    // Note the spelling: KERN_CPTIME on OpenBSD (long[CPUSTATES]),
    // not FreeBSD's KERN_CP_TIME.
    mib[0] = CTL_KERN;
    mib[1] = KERN_CPTIME;
  } else if (std::strcmp(name, "vm.uvmexp") == 0) {
    mib[0] = CTL_VM;
    mib[1] = VM_UVMEXP;
  } else {
    return -1;
  }
  return sysctl(mib, 2, oldp, oldlenp, newp, newlen);
}
#define sysctlbyname openbsd_sysctlbyname
#endif

namespace wilfred {
namespace {

bool g_net = true;
std::mutex weather_mu;
std::string weather_cache;
std::int64_t weather_cache_at{0};

static SearchResult card(const std::string& title, const std::string& sub,
                         const std::string& payload, const std::string& label, int score = 10000,
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
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
    s.erase(s.begin());
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t'))
    s.pop_back();
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
  BOOL ok =
      WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
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
  std::string cmd =
      std::string("curl -fsS --max-time 2 -A Wilfred/1.0 \"") + url + "\" 2>/dev/null";
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

enum class SpeedPhase { Idle, Ping, Download, Upload, Done, Failed };

struct SpeedState {
  SpeedPhase phase{SpeedPhase::Idle};
  bool running{false};
  double ping_ms{0};
  double down_mbps{0};
  double up_mbps{0};
  std::uint64_t down_bytes{0};
  std::uint64_t up_bytes{0};
  std::string error;
  std::string session;
  std::int64_t last_query_ms{0};
  std::int64_t finished_ms{0};
};

std::mutex speed_mu;
SpeedState g_speed;
std::atomic<bool> speed_cancel{false};
std::atomic<bool> speed_busy{false};

static double mbps_of(std::uint64_t bytes, double sec) {
  if (sec < 0.05) return 0;
  return (static_cast<double>(bytes) * 8.0) / (sec * 1000000.0);
}

static std::string fmt_mbps(double v) {
  char buf[48];
  if (v <= 0.0) return "—";
  if (v < 10.0)
    std::snprintf(buf, sizeof(buf), "%.2f Mbps", v);
  else if (v < 100.0)
    std::snprintf(buf, sizeof(buf), "%.1f Mbps", v);
  else
    std::snprintf(buf, sizeof(buf), "%.0f Mbps", v);
  return buf;
}

static std::string fmt_ping(double ms) {
  char buf[48];
  if (ms <= 0.0) return "—";
  if (ms < 10.0)
    std::snprintf(buf, sizeof(buf), "%.1f ms", ms);
  else
    std::snprintf(buf, sizeof(buf), "%.0f ms", ms);
  return buf;
}

static void speed_set_down(std::uint64_t bytes, double mbps) {
  std::lock_guard<std::mutex> lock(speed_mu);
  g_speed.down_bytes = bytes;
  if (mbps > 0) g_speed.down_mbps = mbps;
}

static void speed_set_up(std::uint64_t bytes, double mbps) {
  std::lock_guard<std::mutex> lock(speed_mu);
  g_speed.up_bytes = bytes;
  if (mbps > 0) g_speed.up_mbps = mbps;
}

#ifdef _WIN32
static void speed_winhttp_tune(HINTERNET ses, int timeout_ms) {
  WinHttpSetTimeouts(ses, 4000, 4000, timeout_ms, timeout_ms);
  DWORD proto = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
  proto |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
#endif
  WinHttpSetOption(ses, WINHTTP_OPTION_SECURE_PROTOCOLS, &proto, sizeof(proto));
}

static bool speed_http_get_count(const wchar_t* host, const wchar_t* path, std::uint64_t max_bytes,
                                 int timeout_ms, std::uint64_t* out_bytes, double* out_sec,
                                 bool live_down) {
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return false;
  speed_winhttp_tune(ses, timeout_ms);
  HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return false;
  }
  HINTERNET req = WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return false;
  }
  auto t0 = std::chrono::steady_clock::now();
  BOOL ok =
      WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (ok) ok = WinHttpReceiveResponse(req, nullptr);
  std::uint64_t n = 0;
  if (ok) {
    DWORD avail = 0;
    while (!speed_cancel.load() && WinHttpQueryDataAvailable(req, &avail)) {
      if (!avail) break;
      DWORD want = avail;
      if (max_bytes && n + want > max_bytes) want = static_cast<DWORD>(max_bytes - n);
      std::string chunk(want, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(req, chunk.data(), want, &read) || !read) break;
      n += read;
      double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      if (live_down) speed_set_down(n, mbps_of(n, sec));
      if (max_bytes && n >= max_bytes) break;
      auto elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0)
              .count();
      if (elapsed_ms > timeout_ms) break;
    }
  }
  double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  if (out_bytes) *out_bytes = n;
  if (out_sec) *out_sec = sec;
  return ok && n > 0;
}

static bool speed_http_post_count(const wchar_t* host, const wchar_t* path, std::uint64_t total,
                                  int timeout_ms, std::uint64_t* out_bytes, double* out_sec) {
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return false;
  speed_winhttp_tune(ses, timeout_ms);
  HINTERNET con = WinHttpConnect(ses, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return false;
  }
  HINTERNET req = WinHttpOpenRequest(con, L"POST", path, nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return false;
  }
  wchar_t hdr[] = L"Content-Type: application/octet-stream\r\n";
  auto t0 = std::chrono::steady_clock::now();
  BOOL ok = WinHttpSendRequest(req, hdr, static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0,
                               static_cast<DWORD>(total), 0);
  std::uint64_t n = 0;
  if (ok) {
    std::string chunk(65536, '\0');
    while (!speed_cancel.load() && n < total) {
      DWORD want = static_cast<DWORD>(chunk.size());
      if (n + want > total) want = static_cast<DWORD>(total - n);
      DWORD wrote = 0;
      if (!WinHttpWriteData(req, chunk.data(), want, &wrote) || !wrote) break;
      n += wrote;
      double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      speed_set_up(n, mbps_of(n, sec));
      auto elapsed_ms =
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0)
              .count();
      if (elapsed_ms > timeout_ms) break;
    }
    WinHttpReceiveResponse(req, nullptr);
  }
  double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  if (out_bytes) *out_bytes = n;
  if (out_sec) *out_sec = sec;
  return ok && n > 0;
}
#else
static bool speed_http_get_count(const char* url, std::uint64_t max_bytes, int timeout_ms,
                                 std::uint64_t* out_bytes, double* out_sec, bool ping) {
  char cmd[768];
  int cap = timeout_ms / 1000;
  if (cap < 2) cap = 2;
  std::snprintf(cmd, sizeof(cmd),
                "curl -fsSN --max-time %d -A Wilfred/1.0 \"%s\" 2>/dev/null", cap, url);
  FILE* f = popen(cmd, "r");
  if (!f) return false;
  auto t0 = std::chrono::steady_clock::now();
  std::uint64_t n = 0;
  char buf[65536];
  while (!speed_cancel.load()) {
    size_t got = fread(buf, 1, sizeof(buf), f);
    if (!got) break;
    n += got;
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    if (!ping) speed_set_down(n, mbps_of(n, sec));
    if (max_bytes && n >= max_bytes) break;
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0)
            .count();
    if (elapsed_ms > timeout_ms) break;
  }
  double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  pclose(f);
  if (out_bytes) *out_bytes = n;
  if (out_sec) *out_sec = sec;
  return n > 0;
}

static bool speed_http_post_count(const char* url, std::uint64_t total, int timeout_ms,
                                  std::uint64_t* out_bytes, double* out_sec) {
  char cmd[768];
  int cap = timeout_ms / 1000;
  if (cap < 2) cap = 2;
  std::snprintf(cmd, sizeof(cmd),
                "curl -fsS --max-time %d -A Wilfred/1.0 -X POST "
                "-H \"Content-Type: application/octet-stream\" --data-binary @- -o /dev/null "
                "\"%s\" 2>/dev/null",
                cap, url);
  FILE* f = popen(cmd, "w");
  if (!f) return false;
  auto t0 = std::chrono::steady_clock::now();
  std::uint64_t n = 0;
  std::string chunk(65536, '\0');
  while (!speed_cancel.load() && n < total) {
    size_t want = chunk.size();
    if (n + want > total) want = static_cast<size_t>(total - n);
    size_t w = fwrite(chunk.data(), 1, want, f);
    if (!w) break;
    n += w;
    double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    speed_set_up(n, mbps_of(n, sec));
    auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0)
            .count();
    if (elapsed_ms > timeout_ms) break;
  }
  double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
  pclose(f);
  if (out_bytes) *out_bytes = n;
  if (out_sec) *out_sec = sec;
  return n > 0;
}
#endif

static void run_speed_test() {
  speed_cancel = false;
  {
    std::lock_guard<std::mutex> lock(speed_mu);
    g_speed.running = true;
    g_speed.phase = SpeedPhase::Ping;
    g_speed.ping_ms = 0;
    g_speed.down_mbps = 0;
    g_speed.up_mbps = 0;
    g_speed.down_bytes = 0;
    g_speed.up_bytes = 0;
    g_speed.error.clear();
    g_speed.finished_ms = 0;
  }

  const std::uint64_t down_target = 25ull * 1024ull * 1024ull;
  const std::uint64_t up_target = 8ull * 1024ull * 1024ull;
  const int phase_ms = 12000;

  std::uint64_t bytes = 0;
  double sec = 0;
#ifdef _WIN32
  if (!speed_http_get_count(L"speed.cloudflare.com", L"/__down?bytes=1000", 1000, 4000, &bytes, &sec,
                            false)) {
#else
  if (!speed_http_get_count("https://speed.cloudflare.com/__down?bytes=1000", 1000, 4000, &bytes,
                            &sec, true)) {
#endif
    std::lock_guard<std::mutex> lock(speed_mu);
    g_speed.running = false;
    g_speed.phase = SpeedPhase::Failed;
    g_speed.error = "Could not reach the speed test server";
    g_speed.finished_ms = unix_millis();
    speed_busy = false;
    return;
  }
  {
    std::lock_guard<std::mutex> lock(speed_mu);
    g_speed.ping_ms = sec * 1000.0;
    g_speed.phase = SpeedPhase::Download;
  }

  bytes = 0;
  sec = 0;
#ifdef _WIN32
  speed_http_get_count(L"speed.cloudflare.com", L"/__down?bytes=25000000", down_target, phase_ms,
                       &bytes, &sec, true);
#else
  speed_http_get_count("https://speed.cloudflare.com/__down?bytes=25000000", down_target, phase_ms,
                       &bytes, &sec, false);
#endif
  if (!speed_cancel.load()) {
    std::lock_guard<std::mutex> lock(speed_mu);
    g_speed.down_bytes = bytes;
    g_speed.down_mbps = mbps_of(bytes, sec);
    g_speed.phase = SpeedPhase::Upload;
  }

  bytes = 0;
  sec = 0;
#ifdef _WIN32
  bool up_ok = speed_http_post_count(L"speed.cloudflare.com", L"/__up", up_target, phase_ms, &bytes,
                                    &sec);
#else
  bool up_ok =
      speed_http_post_count("https://speed.cloudflare.com/__up", up_target, phase_ms, &bytes, &sec);
#endif
  {
    std::lock_guard<std::mutex> lock(speed_mu);
    g_speed.up_bytes = bytes;
    if (up_ok) g_speed.up_mbps = mbps_of(bytes, sec);
    g_speed.running = false;
    g_speed.finished_ms = unix_millis();
    if (speed_cancel.load()) {
      g_speed.phase = SpeedPhase::Failed;
      g_speed.error = "Cancelled";
    } else if (g_speed.down_mbps <= 0) {
      g_speed.phase = SpeedPhase::Failed;
      g_speed.error = "Download failed";
    } else {
      g_speed.phase = SpeedPhase::Done;
      if (!up_ok) g_speed.error = "Upload failed";
    }
  }
  speed_busy = false;
}

static void kick_speed_test(bool force, const std::string& session) {
  auto now = unix_millis();
  bool start = false;
  {
    std::lock_guard<std::mutex> lock(speed_mu);
    g_speed.last_query_ms = now;
    if (g_speed.running || speed_busy.load()) return;
    if (g_speed.finished_ms &&
        (g_speed.phase == SpeedPhase::Done || g_speed.phase == SpeedPhase::Failed)) {
      if (g_speed.session == session) return;
      if (!force) return;
    }
    if (!g_net) return;
    g_speed.session = session;
    g_speed.running = true;
    g_speed.phase = SpeedPhase::Ping;
    speed_busy = true;
    start = true;
  }
  if (!start) return;
  try {
    std::thread(run_speed_test).detach();
  } catch (...) {
    std::lock_guard<std::mutex> lock(speed_mu);
    g_speed.phase = SpeedPhase::Failed;
    g_speed.error = "Could not start speed test";
    g_speed.running = false;
    speed_busy = false;
  }
}

static std::vector<SearchResult> speed_cards() {
  SpeedState s;
  {
    std::lock_guard<std::mutex> lock(speed_mu);
    s = g_speed;
  }
  std::vector<SearchResult> out;
  if (!g_net) {
    out.push_back(card("Speed test", "Network checks are disabled", "speedtest", "speedtest"));
    return out;
  }
  if (s.phase == SpeedPhase::Failed && !s.error.empty() && s.down_mbps <= 0) {
    out.push_back(card("Speed test failed", s.error, s.error, "speedtest"));
    return out;
  }

  const char* phase = "Starting…";
  int meter = 4;
  if (s.phase == SpeedPhase::Ping) {
    phase = "Measuring ping…";
    meter = 8;
  } else if (s.phase == SpeedPhase::Download) {
    phase = "Downloading… live";
    meter = 12 + static_cast<int>(std::min(48.0, s.down_bytes / (25000000.0 / 48.0)));
  } else if (s.phase == SpeedPhase::Upload) {
    phase = "Uploading… live";
    meter = 60 + static_cast<int>(std::min(35.0, s.up_bytes / (8000000.0 / 35.0)));
  } else if (s.phase == SpeedPhase::Done) {
    phase = s.error.empty() ? "Done · enter copies" : "Done · upload skipped";
    meter = 100;
  } else if (s.phase == SpeedPhase::Idle) {
    phase = "Starting speed test…";
    meter = 2;
  }

  std::string summary = "↓ " + fmt_mbps(s.down_mbps) + "   ↑ " + fmt_mbps(s.up_mbps);
  std::string copy = "Download " + fmt_mbps(s.down_mbps) + " · Upload " + fmt_mbps(s.up_mbps) +
                     " · Ping " + fmt_ping(s.ping_ms);
  out.push_back(card(summary, std::string("Ping ") + fmt_ping(s.ping_ms) + " · " + phase, copy,
                     "speedtest", 10000, ResultAction::Copy, meter));
  out.push_back(card("Download  " + fmt_mbps(s.down_mbps),
                     s.phase == SpeedPhase::Download
                         ? human_bytes(s.down_bytes) + " received"
                         : (s.phase == SpeedPhase::Idle || s.phase == SpeedPhase::Ping
                                ? "Waiting…"
                                : "Peak throughput"),
                     copy, "speedtest", 9990, ResultAction::Copy,
                     s.phase == SpeedPhase::Download ? meter : (s.down_mbps > 0 ? 100 : 0)));
  out.push_back(card("Upload  " + fmt_mbps(s.up_mbps),
                     s.phase == SpeedPhase::Upload
                         ? human_bytes(s.up_bytes) + " sent"
                         : (s.phase == SpeedPhase::Done || s.up_mbps > 0 ? "Peak throughput"
                                                                        : "Waiting…"),
                     copy, "speedtest", 9980, ResultAction::Copy,
                     s.phase == SpeedPhase::Upload ? meter : (s.up_mbps > 0 ? 100 : 0)));
  return out;
}

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
  while (!body.empty() && (body.back() == '\n' || body.back() == '\r'))
    body.pop_back();
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

#if defined(WILFRED_BSD)
// Seconds since boot via kern.boottime (no /proc on BSD). Returns 0 when
// the sysctl is unavailable; callers fall back to omitting the value.
static std::uint64_t bsd_uptime_sec() {
  struct timeval boot{};
  std::size_t sz = sizeof(boot);
  if (sysctlbyname("kern.boottime", &boot, &sz, nullptr, 0) != 0) return 0;
  std::time_t now = std::time(nullptr);
  if (now < boot.tv_sec) return 0;
  return static_cast<std::uint64_t>(now - boot.tv_sec);
}
#endif

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
#elif defined(WILFRED_BSD)
  // hw.physmem exists on every BSD; `used` needs a free-page counter.
  // FreeBSD reports it exactly, the others below are validated best
  // efforts that fall back to total-only (like macOS) on any mismatch.
  unsigned long phys = 0;
  std::size_t sz = sizeof(phys);
  if (sysctlbyname("hw.physmem", &phys, &sz, nullptr, 0) == 0) total = phys;
#if defined(__FreeBSD__) || defined(__DragonFly__)
  // vm.stats.vm.v_free_count is FreeBSD-documented; DragonFly shares the
  // FreeBSD-derived VM sysctl tree, otherwise this quietly no-ops.
  unsigned free_pages = 0;
  sz = sizeof(free_pages);
  long page = sysconf(_SC_PAGESIZE);
  if (total && page > 0 &&
      sysctlbyname("vm.stats.vm.v_free_count", &free_pages, &sz, nullptr, 0) == 0) {
    std::uint64_t freeb = static_cast<std::uint64_t>(free_pages) * static_cast<std::uint64_t>(page);
    if (freeb < total) used = total - freeb;
  }
#elif defined(__OpenBSD__) || defined(__NetBSD__)
  // vm.uvmexp: npages/free are page counts (cf. htop's OpenBSD backend).
  struct uvmexp uv{};
  std::size_t uvsz = sizeof(uv);
  long page = sysconf(_SC_PAGESIZE);
  if (total && page > 0 && sysctlbyname("vm.uvmexp", &uv, &uvsz, nullptr, 0) == 0 &&
      uv.npages > 0 && uv.free >= 0 && uv.free <= uv.npages) {
    used =
        static_cast<std::uint64_t>(uv.npages - uv.free) * static_cast<std::uint64_t>(page);
  }
#endif
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
#elif defined(WILFRED_BSD)
  // kern.cp_time: cumulative ticks per CPU state, idle last on every
  // supported BSD. Two samples 80ms apart, mirroring the /proc/stat path.
  // Entry size is checked (not assumed) so a different state count can only
  // refuse, never misread. Returns -1 when unavailable.
  static unsigned long long prev_idle = 0, prev_total = 0;
  auto read_times = []() -> std::pair<unsigned long long, unsigned long long> {
    unsigned long long st[8]{};
    std::size_t sz = sizeof(st);
    if (sysctlbyname("kern.cp_time", st, &sz, nullptr, 0) != 0) return {0, 0};
    if (sz % sizeof(st[0]) != 0) return {0, 0};
    std::size_t n = sz / sizeof(st[0]);
    if (n < 5) return {0, 0};
    unsigned long long total = 0;
    for (std::size_t i = 0; i < n; ++i) total += st[i];
    return {st[n - 1], total};
  };
  auto [idle_all, total] = read_times();
  if (!total) return -1;
  if (!prev_total) {
    prev_idle = idle_all;
    prev_total = total;
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    std::tie(idle_all, total) = read_times();
    if (!total) return -1;
  }
  auto didle = idle_all - prev_idle;
  auto dtot = total - prev_total;
  prev_idle = idle_all;
  prev_total = total;
  if (!dtot) return 0;
  return std::clamp(100.0 * (1.0 - static_cast<double>(didle) / static_cast<double>(dtot)), 0.0,
                    100.0);
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
      HANDLE h = OpenProcess(
          PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE,
          pe.th32ProcessID);
      if (h) {
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc))) p.working_set = pmc.WorkingSetSize;
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
#elif defined(__FreeBSD__)
  // No /proc by default: enumerate via sysctl KERN_PROC_PROC. The other
  // BSDs have their own branches below; remaining Unix keeps the /proc
  // scan (empty without procfs).
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
  std::size_t len = 0;
  if (sysctl(mib, 4, nullptr, &len, nullptr, 0) == 0 && len > 0) {
    std::vector<char> buf(len + 16);
    if (sysctl(mib, 4, buf.data(), &len, nullptr, 0) == 0 && len >= sizeof(kinfo_proc)) {
      long page = sysconf(_SC_PAGESIZE);
      if (page <= 0) page = 4096;
      std::size_t n = len / sizeof(kinfo_proc);
      auto* kp = reinterpret_cast<kinfo_proc*>(buf.data());
      for (std::size_t i = 0; i < n; ++i) {
        if (kp[i].ki_pid <= 0) continue;
        std::string name(kp[i].ki_comm, strnlen(kp[i].ki_comm, sizeof(kp[i].ki_comm)));
        if (name.empty()) continue;
        auto folded = to_lower_utf8(name);
        if (!want.empty() && folded.find(want) == std::string::npos) continue;
        ProcInfo p;
        p.pid = static_cast<std::uint32_t>(kp[i].ki_pid);
        p.name = name;
        p.working_set =
            static_cast<std::uint64_t>(kp[i].ki_rssize) * static_cast<std::uint64_t>(page);
        p.threads = static_cast<std::uint32_t>(kp[i].ki_numthreads);
        out.push_back(std::move(p));
      }
    }
  }
#elif defined(__OpenBSD__)
  // sysctl KERN_PROC_ALL returns an array of struct kinfo_proc
  // (see sysctl(3)); p_vm_rssize is a page count, same as htop uses it.
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
  std::size_t len = 0;
  if (sysctl(mib, 4, nullptr, &len, nullptr, 0) == 0 && len > 0) {
    std::vector<char> buf(len + 16);
    if (sysctl(mib, 4, buf.data(), &len, nullptr, 0) == 0 && len >= sizeof(kinfo_proc)) {
      long page = sysconf(_SC_PAGESIZE);
      if (page <= 0) page = 4096;
      std::size_t n = len / sizeof(kinfo_proc);
      auto* kp = reinterpret_cast<kinfo_proc*>(buf.data());
      for (std::size_t i = 0; i < n; ++i) {
        if (kp[i].p_pid <= 0) continue;
        std::string name(kp[i].p_comm, strnlen(kp[i].p_comm, sizeof(kp[i].p_comm)));
        if (name.empty()) continue;
        auto folded = to_lower_utf8(name);
        if (!want.empty() && folded.find(want) == std::string::npos) continue;
        ProcInfo p;
        p.pid = static_cast<std::uint32_t>(kp[i].p_pid);
        p.name = name;
        p.working_set =
            static_cast<std::uint64_t>(kp[i].p_vm_rssize) * static_cast<std::uint64_t>(page);
        out.push_back(std::move(p));
      }
    }
  }
#elif defined(__NetBSD__)
  // KERN_PROC2 takes a 6-element MIB: op, id, element size, max count
  // (0 = no limit). kinfo_proc2 carries p_pid/p_comm plus p_vm_rssize
  // in pages.
  int mib[6] = {CTL_KERN, KERN_PROC2, KERN_PROC_ALL, 0, (int)sizeof(struct kinfo_proc2), 0};
  std::size_t len = 0;
  if (sysctl(mib, 6, nullptr, &len, nullptr, 0) == 0 && len > 0) {
    std::vector<char> buf(len + 16);
    if (sysctl(mib, 6, buf.data(), &len, nullptr, 0) == 0 &&
        len >= sizeof(struct kinfo_proc2)) {
      long page = sysconf(_SC_PAGESIZE);
      if (page <= 0) page = 4096;
      std::size_t n = len / sizeof(struct kinfo_proc2);
      auto* kp = reinterpret_cast<struct kinfo_proc2*>(buf.data());
      for (std::size_t i = 0; i < n; ++i) {
        if (kp[i].p_pid <= 0) continue;
        std::string name(kp[i].p_comm, strnlen(kp[i].p_comm, sizeof(kp[i].p_comm)));
        if (name.empty()) continue;
        auto folded = to_lower_utf8(name);
        if (!want.empty() && folded.find(want) == std::string::npos) continue;
        ProcInfo p;
        p.pid = static_cast<std::uint32_t>(kp[i].p_pid);
        p.name = name;
        p.working_set =
            static_cast<std::uint64_t>(kp[i].p_vm_rssize) * static_cast<std::uint64_t>(page);
        out.push_back(std::move(p));
      }
    }
  }
#elif defined(__DragonFly__)
  // KERN_PROC_ALL returns struct kinfo_proc (see sys/kinfo.h via
  // <sys/user.h>); kp_vm_rssize is in pages, kp_nthreads is exact.
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0};
  std::size_t len = 0;
  if (sysctl(mib, 4, nullptr, &len, nullptr, 0) == 0 && len > 0) {
    std::vector<char> buf(len + 16);
    if (sysctl(mib, 4, buf.data(), &len, nullptr, 0) == 0 && len >= sizeof(kinfo_proc)) {
      long page = sysconf(_SC_PAGESIZE);
      if (page <= 0) page = 4096;
      std::size_t n = len / sizeof(kinfo_proc);
      auto* kp = reinterpret_cast<kinfo_proc*>(buf.data());
      for (std::size_t i = 0; i < n; ++i) {
        if (kp[i].kp_pid <= 0) continue;
        std::string name(kp[i].kp_comm, strnlen(kp[i].kp_comm, sizeof(kp[i].kp_comm)));
        if (name.empty()) continue;
        auto folded = to_lower_utf8(name);
        if (!want.empty() && folded.find(want) == std::string::npos) continue;
        ProcInfo p;
        p.pid = static_cast<std::uint32_t>(kp[i].kp_pid);
        p.name = name;
        p.working_set =
            static_cast<std::uint64_t>(kp[i].kp_vm_rssize) * static_cast<std::uint64_t>(page);
        p.threads = static_cast<std::uint32_t>(kp[i].kp_nthreads);
        out.push_back(std::move(p));
      }
    }
  }
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
#ifdef WILFRED_BSD
          double uptime = static_cast<double>(bsd_uptime_sec());
#else
          std::ifstream up("/proc/uptime");
          double uptime = 1;
          up >> uptime;
#endif
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
  GetAdaptersAddresses(AF_INET,
                       GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                       nullptr, nullptr, &sz);
  if (!sz) return {};
  std::vector<char> buf(sz);
  auto* addrs = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data());
  if (GetAdaptersAddresses(
          AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
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
#if defined(WILFRED_BSD)
  // hostname(1) has no -I flag on BSD; enumerate interfaces directly.
  ifaddrs* list = nullptr;
  if (getifaddrs(&list) != 0) return {};
  std::string ip;
  for (auto* a = list; a; a = a->ifa_next) {
    if (!a->ifa_addr || a->ifa_addr->sa_family != AF_INET) continue;
    if (!(a->ifa_flags & IFF_UP) || (a->ifa_flags & IFF_LOOPBACK)) continue;
    char buf[INET_ADDRSTRLEN]{};
    auto* sin = reinterpret_cast<sockaddr_in*>(a->ifa_addr);
    if (inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf))) ip = buf;
    if (!ip.empty()) break;
  }
  freeifaddrs(list);
  return ip;
#else
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
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    s.pop_back();
  return s;
#endif
}
#endif

}  // namespace

void set_mini_network_enabled(bool enabled) {
  g_net = enabled;
}

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
  if (key == "weather" || key == "wttr" || key == "forecast")
    set(MiniKind::Weather, rest.empty());
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
  else if (key == "power" && (rest == "off" || rest == "down")) {
    it.kind = MiniKind::System;
    it.remainder = "shutdown";
    it.exact = true;
  } else if (key == "battery" || key == "power")
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
  else if (key == "bm" || key == "bookmarks" || key == "bookmark" || key == "tabs" ||
           key == "hist" || key == "history")
    set(MiniKind::Browser, true);
  else if (key == "os" || key == "systeminfo" || key == "sysinfo")
    set(MiniKind::Os, true);
  else if (key == "cores" || key == "nproc" || key == "threads")
    set(MiniKind::Cores, true);
  else if (key == "screenshot" || key == "screenshots" || key == "screencap" ||
           key == "screencapture" || key == "printscreen") {
    if (rest.empty()) {
      set(MiniKind::Screenshot, true);
    } else {
      ScreenshotMode m;
      if (parse_screenshot_mode(rest, m))
        set(MiniKind::Screenshot, true);
      else
        return it;
    }
  } else if (l == "print screen" || l.rfind("print screen ", 0) == 0 ||
             l.rfind("print screen:", 0) == 0 || l == "screen capture" ||
             l.rfind("screen capture ", 0) == 0 || l.rfind("screen capture:", 0) == 0 ||
             l == "screen shot" || l.rfind("screen shot ", 0) == 0 ||
             l.rfind("screen shot:", 0) == 0 || l == "capture screen" ||
             l.rfind("capture screen ", 0) == 0 || l.rfind("capture screen:", 0) == 0) {
    const char* prefixes[] = {"print screen", "screen capture", "screen shot", "capture screen",
                              nullptr};
    std::string mode_rest;
    for (auto** p = prefixes; *p; ++p) {
      std::string pl(*p);
      if (l == pl) {
        mode_rest.clear();
        break;
      }
      if (l.rfind(pl + " ", 0) == 0) {
        mode_rest = trim_sv(s.substr(pl.size()));
        break;
      }
      if (l.rfind(pl + ":", 0) == 0) {
        mode_rest = trim_sv(s.substr(pl.size() + 1));
        break;
      }
    }
    if (mode_rest.empty()) {
      it.kind = MiniKind::Screenshot;
      it.remainder.clear();
      it.exact = true;
    } else {
      ScreenshotMode m;
      if (!parse_screenshot_mode(mode_rest, m)) return it;
      it.kind = MiniKind::Screenshot;
      it.remainder = mode_rest;
      it.exact = true;
    }
  } else if (key == "screen" || key == "resolution" || key == "display")
    set(MiniKind::Screen, true);
  else if (key == "swap" || key == "pagefile" || key == "vmem")
    set(MiniKind::Swap, true);
  else if (key == "help" || key == "minis" || key == "cmds" || key == "commands")
    set(MiniKind::Help, true);
  else if (key == "speedtest" || key == "speed-test" || key == "speed_test" || key == "netspeed" ||
           key == "bandwidth" || key == "internetspeed") {
    auto r = to_lower_utf8(rest);
    if (rest.empty() || r == "again" || r == "retry" || r == "new" || r == "rerun")
      set(MiniKind::Speedtest, true);
  }
  else if (key == "macros" || key == "bangs")
    set(MiniKind::MacrosList, true);
  else if (key == "windows" || key == "window" || key == "winswitch" || key == "wswitch" ||
           key == "switchto" || key == "switch")
    set(MiniKind::Windows, rest.empty());
  else if (key == "minimize" || key == "minimise") {
    it.kind = MiniKind::WindowOp;
    it.remainder = rest;
    it.op = "minimize";
    it.exact = false;
  } else if (key == "maximize" || key == "maximise") {
    it.kind = MiniKind::WindowOp;
    it.remainder = rest;
    it.op = "maximize";
    it.exact = false;
  } else if (key == "close" && to_lower_utf8(rest).rfind("window", 0) == 0) {
    it.kind = MiniKind::WindowOp;
    it.remainder = trim_sv(rest.substr(6));
    it.op = "close";
    it.exact = false;
  } else if (key == "emoji" || key == "emojis" || key == "emote" || key == "emotes")
    set(MiniKind::Emoji, true);
  else if (key == "symbol" || key == "symbols" || key == "glyph" || key == "glyphs")
    set(MiniKind::Symbol, true);
  else if (key == "fx" || key == "currency" || key == "forex" || key == "ccy")
    set(MiniKind::Fx, true);
  else if (key == "tz" || key == "timezone" || key == "timezones" || key == "worldclock")
    set(MiniKind::Tz, true);
  else if (key == "color" || key == "colour")
    set(MiniKind::Color, true);
  else if (key == "uuid" || key == "uuid4" || key == "guid" || key == "uuidv4")
    set(MiniKind::Uuid, true);
  else if (key == "base64" || key == "b64" || key == "base64encode" || key == "encode64" ||
           key == "b64e")
    set(MiniKind::Base64, rest.empty());
  else if (key == "base64decode" || key == "base64d" || key == "b64d" || key == "decode64" ||
           key == "b64decode")
    set(MiniKind::Base64, rest.empty());
  else if (key == "sha256")
    set(MiniKind::Sha256, rest.empty());
  else if ((key == "sha" || key == "hash") && !rest.empty())
    set(MiniKind::Sha256, false);
  else if (key == "lorem" || key == "ipsum" || key == "loremipsum")
    set(MiniKind::Lorem, true);
  else if (key == "json" || key == "prettyjson" || key == "jsonfmt")
    set(MiniKind::Json, rest.empty());
  else if (key == "pretty" && !rest.empty())
    set(MiniKind::Json, true);
  else if (key == "lock" || key == "lockscreen") {
    if (rest.empty() || rest == "screen" || rest == "pc" || rest == "computer") {
      it.kind = MiniKind::System;
      it.remainder = "lock";
      it.exact = true;
    }
  } else if (key == "sleep" || key == "suspend") {
    if (rest.empty() || rest == "now" || rest == "computer" || rest == "pc") {
      it.kind = MiniKind::System;
      it.remainder = "sleep";
      it.exact = true;
    }
  } else if (key == "shutdown" || key == "poweroff" || key == "halt" ||
             (key == "shut" && (rest == "down" || rest.rfind("down", 0) == 0)) ||
             (key == "power" && (rest == "off" || rest == "down"))) {
    it.kind = MiniKind::System;
    it.remainder = "shutdown";
    it.exact = true;
  } else if (key == "restart" || key == "reboot") {
    if (rest.empty() || rest == "now" || rest == "computer" || rest == "pc") {
      it.kind = MiniKind::System;
      it.remainder = "restart";
      it.exact = true;
    }
  } else if (key == "logout" || key == "logoff" || key == "signout" ||
             (key == "log" && (rest == "out" || rest == "off")) ||
             (key == "sign" && rest == "out")) {
    it.kind = MiniKind::System;
    it.remainder = "logout";
    it.exact = true;
  } else if (key == "emptytrash" || key == "emptyrecycle" ||
             (key == "empty" && (rest == "trash" || rest == "recycle" || rest == "bin" ||
                                 rest == "recycle bin" || rest == "recyclebin" ||
                                 rest == "the trash" || rest == "the bin"))) {
    it.kind = MiniKind::System;
    it.remainder = "empty_trash";
    it.exact = true;
  } else if (key == "timer" || key == "timers" || key == "pomodoro" || key == "pomo" ||
             key == "alarm" || key == "countdown")
    set(MiniKind::Timer, false);
  else if (key == "stopwatch" || key == "stop-watch" || key == "sw")
    set(MiniKind::Stopwatch, false);
  else if (key == "note" || key == "notes" || key == "notepad" || key == "memo" || key == "memos")
    set(MiniKind::Note, false);
  else if (key == "todo" || key == "todos" || key == "task" || key == "tasks")
    set(MiniKind::Todo, false);
  else if (key == "kill" || key == "killall" || key == "pkill" || key == "taskkill")
    set(MiniKind::Kill, false);
  else if (key == "media" || key == "player" || key == "music" || key == "mediakeys")
    set(MiniKind::Media, false);
  else if (key == "play" || key == "pause" || key == "next" || key == "previous" ||
           key == "prev" || key == "mute" || key == "volume" || key == "vol") {
    // Bare media verbs double as media controls (non-exact so file hits still show).
    if (rest.empty() ||
        to_lower_utf8(rest) == "track" || to_lower_utf8(rest) == "song" ||
        to_lower_utf8(rest) == "music" || to_lower_utf8(rest) == "media" ||
        to_lower_utf8(rest) == "up" || to_lower_utf8(rest) == "down" ||
        to_lower_utf8(rest) == "next" || to_lower_utf8(rest) == "prev") {
      it.kind = MiniKind::Media;
      it.remainder = rest.empty() ? key : (key + " " + rest);
      it.exact = false;
    }
  } else if (key == "ping")
    set(MiniKind::Ping, false);
  else if (key == "dns" || key == "nslookup" || key == "dig" || key == "resolve" ||
           key == "lookup")
    set(MiniKind::Dns, false);
  else if (key == "myip" || key == "publicip" || key == "public-ip" || key == "my-ip" ||
           key == "wanip" || (key == "ip" && (to_lower_utf8(rest) == "public" ||
                                              to_lower_utf8(rest) == "wan" ||
                                              to_lower_utf8(rest) == "external"))) {
    it.kind = MiniKind::MyIp;
    it.remainder = (key == "ip" ? rest : std::string());
    it.exact = true;
  } else if (key == "hex" || key == "dec" || key == "decimal" || key == "bin" ||
             key == "binary" || key == "oct" || key == "octal" || key == "base" ||
             key == "bit" || key == "bits" || key == "bitwise") {
    // Number-base and bit tools also live in math/dev; expose as minis for help.
    it.kind = (key == "bit" || key == "bits" || key == "bitwise") ? MiniKind::Bits : MiniKind::Base;
    it.remainder = s.substr(key.size());
    it.remainder = trim_sv(it.remainder);
    if (!it.remainder.empty() && it.remainder[0] == ':')
      it.remainder = trim_sv(it.remainder.substr(1));
    it.exact = true;
  } else if (key == "regex" || key == "regexp" || key == "re" || key == "regexi" || key == "rei")
    set(MiniKind::Regex, false);
  else if (key == "url" || key == "urlencode" || key == "urlenc" || key == "urldecode" ||
           key == "urldec" || key == "encodeurl" || key == "decodeurl")
    set(MiniKind::UrlCodec, rest.empty());
  else if (key == "jwt" || key == "jwtdecode" || key == "jwt-decode")
    set(MiniKind::Jwt, rest.empty());
  else if (key == "dupes" || key == "dups" || key == "duplicates" || key == "duplicate" ||
           key == "dedup" || key == "dedupe")
    set(MiniKind::Dupes, false);
  else if (key == "large" || key == "largefiles" || key == "large-files" || key == "bigfiles" ||
           key == "big-files" || key == "biggest")
    set(MiniKind::Large, false);
  else if (key == "workflow" || key == "workflows" || key == "flow" || key == "flows" ||
           key == "run" || key == "routines" || key == "routine")
    set(MiniKind::Workflow, false);
  else if (key == "ql" || key == "quicklink" || key == "quicklinks" || key == "links" ||
           key == "link")
    set(MiniKind::Quicklink, false);
  else if (key == "transcribe" || key == "transcribes" || key == "transcription" ||
           key == "stt" || key == "speech-to-text" || key == "speech_to_text")
    set(MiniKind::Transcribe, false);
  else if (key == "dictate" || key == "dictation" || key == "dictating")
    set(MiniKind::Dictate, false);
  else if (key == "layout" || key == "layouts")
    set(MiniKind::Layout, false);
  return it;
}

std::vector<SearchResult> mini_results(const std::string& query, const Config& cfg,
                                       const std::string& clipboard, IndexEngine* index) {
  std::vector<SearchResult> out;
  if (!cfg.search.minis) return out;
  auto intent = parse_mini_intent(query);
  if (intent.kind == MiniKind::None) return out;

  if (intent.kind == MiniKind::Time) {
    if (!intent.remainder.empty()) {
      MathResult m;
      auto q = intent.remainder;
      if (convert_datetime(q, m) || convert_datetime("now in " + q, m)) {
        out.push_back(card(m.display, "Time · " + intent.remainder, m.display, "time"));
      }
    }
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

  if (intent.kind == MiniKind::Speedtest) {
    auto rest = to_lower_utf8(intent.remainder);
    bool again = rest == "again" || rest == "retry" || rest == "new" || rest == "rerun";
    kick_speed_test(again, again ? rest : std::string("run"));
    return speed_cards();
  }

  if (intent.kind == MiniKind::Weather) {
    auto w = fetch_weather(intent.remainder);
    if (!w.empty()) {
      out.push_back(card(w,
                         intent.remainder.empty() ? "Local weather · enter copies"
                                                  : "Weather · " + intent.remainder,
                         w, "weather"));
    } else {
      std::string url = "https://wttr.in/";
      if (!intent.remainder.empty()) url += percent_encode(intent.remainder);
      SearchResult r =
          card("Weather", "Open live forecast", url, "weather", 9900, ResultAction::WebSearch);
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
      auto r = card(line, d.name, line, intent.kind == MiniKind::DiskUsage ? "disku" : "disk",
                    10000, ResultAction::Copy, pct);
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
      out.insert(out.begin(),
                 card(sum, "Total disk usage", sum, "disku", 10050, ResultAction::Copy, pct));
    }
    return out;
  }

  if (intent.kind == MiniKind::Windows) {
    auto wins = native_list_windows();
    auto needle = fold_search(intent.remainder);
    std::vector<SearchResult> hits;
    int idx = 0;
    for (auto& w : wins) {
      SearchResult r;
      r.title = w.title;
      r.subtitle = w.owner.empty() ? "Open window" : w.owner;
      r.payload = std::to_string(w.id);
      r.path = r.payload;
      r.action = ResultAction::SwitchWindow;
      r.kind_label = "window";
      r.category = "window";
      if (needle.empty()) {
        r.score = 10000 - idx;
      } else {
        auto title_sc = score_fuzzy(needle, fold_search(w.title), w.title);
        auto owner_sc = score_fuzzy(needle, fold_search(w.owner), w.owner);
        int sc = std::max(title_sc.score, owner_sc.score);
        if (!title_sc.matched && !owner_sc.matched &&
            to_lower_utf8(w.title).find(to_lower_utf8(intent.remainder)) == std::string::npos &&
            to_lower_utf8(w.owner).find(to_lower_utf8(intent.remainder)) == std::string::npos)
          continue;
        r.score = 10000 + sc;
      }
      hits.push_back(std::move(r));
      ++idx;
    }
    if (hits.empty()) {
      out.push_back(card(intent.remainder.empty() ? "No open windows" : "No matching window",
                         intent.remainder.empty()
                             ? "Type windows to list them"
                             : "No window matching \"" + intent.remainder + "\"",
                         "", "window", 9000, ResultAction::None));
      return out;
    }
    std::sort(hits.begin(), hits.end(), [](auto& a, auto& b) { return a.score > b.score; });
    if (hits.size() > 16) hits.resize(16);
    return hits;
  }

  if (intent.kind == MiniKind::WindowOp) {
    auto op = intent.op;
    std::string verb = op == "minimize" ? "Minimize" : op == "maximize" ? "Maximize" : "Close";
    auto wins = native_list_windows();
    auto needle = fold_search(intent.remainder);
    int idx = 0;
    for (auto& w : wins) {
      if (!needle.empty()) {
        auto title_sc = score_fuzzy(needle, fold_search(w.title), w.title);
        auto owner_sc = score_fuzzy(needle, fold_search(w.owner), w.owner);
        if (!title_sc.matched && !owner_sc.matched &&
            to_lower_utf8(w.title).find(to_lower_utf8(intent.remainder)) == std::string::npos &&
            to_lower_utf8(w.owner).find(to_lower_utf8(intent.remainder)) == std::string::npos)
          continue;
      }
      SearchResult r;
      r.title = verb + " " + w.title;
      r.subtitle = w.owner.empty() ? "Window · enter runs" : w.owner + " · enter runs";
      r.payload = std::to_string(w.id);
      r.path = r.payload;
      r.action = ResultAction::Copy;
      r.score = 10000 - idx;
      r.kind_label = "window";
      r.category = "window";
      r.actions.clear();
      r.actions.push_back({"window_" + op, verb});
      r.actions.push_back({"open", "Switch"});
      r.actions.push_back({"copy_name", "Copy title"});
      out.push_back(std::move(r));
      ++idx;
      if (idx >= 8) break;
    }
    if (out.empty()) {
      out.push_back(card(intent.remainder.empty() ? "No open windows" : "No matching window",
                         "Type minimize <name>, maximize <name>, or close window <name>", "",
                         "window", 9000, ResultAction::None));
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
      SearchResult r = card(title, sub, title, "process", 10000, ResultAction::Copy, meter);
      r.id = p.pid;
      r.category = "process";
      r.actions.clear();
      r.actions.push_back({"copy_text", "Copy"});
      r.actions.push_back({"kill_process", "Kill process"});
      out.push_back(std::move(r));
    }
    return out;
  }

  if (intent.kind == MiniKind::Kill) {
    auto target = trim_sv(intent.remainder);
    if (target.empty()) {
      out.push_back(card("Kill a process", "Type kill <pid or name> · lists matches first", "",
                         "kill", 8000, ResultAction::None));
      return out;
    }
    // Numeric PID: direct kill card.
    bool numeric = !target.empty();
    for (char c : target)
      if (!std::isdigit(static_cast<unsigned char>(c))) numeric = false;
    // Allow "pid 1234" prefix.
    std::string needle = target;
    auto tl = to_lower_utf8(target);
    if (tl.rfind("pid ", 0) == 0) {
      needle = trim_sv(target.substr(4));
      numeric = !needle.empty();
      for (char c : needle)
        if (!std::isdigit(static_cast<unsigned char>(c))) numeric = false;
    }
    if (numeric) {
      std::uint32_t pid = static_cast<std::uint32_t>(std::stoul(needle));
      SearchResult r = card("Kill PID " + std::to_string(pid),
                            "Terminate process " + std::to_string(pid) + " · enter kills",
                            "pid:" + std::to_string(pid), "kill", 10000, ResultAction::Copy);
      r.id = pid;
      r.category = "process";
      r.actions.clear();
      r.actions.push_back({"kill_process", "Kill process"});
      r.actions.push_back({"copy_text", "Copy"});
      out.push_back(std::move(r));
      return out;
    }
    auto procs = list_processes(needle);
    if (procs.empty()) {
      out.push_back(card("No matching process", needle, "", "kill", 9000, ResultAction::None));
      return out;
    }
    for (auto& p : procs) {
      char title[192];
      std::snprintf(title, sizeof(title), "Kill %s (PID %u)  ·  RAM %s", p.name.c_str(), p.pid,
                    human_bytes(p.working_set).c_str());
      SearchResult r = card(title, "Enter terminates this process", "pid:" + std::to_string(p.pid),
                            "kill", 10000, ResultAction::Copy);
      r.id = p.pid;
      r.category = "process";
      r.actions.clear();
      r.actions.push_back({"kill_process", "Kill process"});
      r.actions.push_back({"copy_text", "Copy"});
      out.push_back(std::move(r));
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
        std::snprintf(title, sizeof(title), "Battery  %d%%  ·  %s", (int)st.BatteryLifePercent,
                      src);
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
      out.push_back(
          card("Battery", "Unavailable on this platform", "", "battery", 8000, ResultAction::None));
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
#elif defined(WILFRED_BSD)
    auto sec = bsd_uptime_sec();
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
    auto filter = trim_sv(intent.remainder);
    if (to_lower_utf8(filter) == "clear") {
      SearchResult r =
          card("Clear clipboard history", "Remove all unpinned clips", "clip:clear", "clips",
               10000, ResultAction::None);
      r.category = "clips";
      r.actions.push_back({"open", "Clear"});
      out.push_back(std::move(r));
      return out;
    }
    // Typed search: `clips url [query]`, `clips email`, `clips path`, `clips code`,
    // `clips ip`, `clips text`. First token is the type when it matches.
    std::string type_filter;
    std::string text_filter = filter;
    {
      auto sp = filter.find(' ');
      std::string first = sp == std::string::npos ? filter : filter.substr(0, sp);
      auto fl = to_lower_utf8(first);
      if (fl == "url" || fl == "urls" || fl == "link" || fl == "links") {
        type_filter = "url";
        text_filter = sp == std::string::npos ? "" : trim_sv(filter.substr(sp + 1));
      } else if (fl == "email" || fl == "emails" || fl == "mail") {
        type_filter = "email";
        text_filter = sp == std::string::npos ? "" : trim_sv(filter.substr(sp + 1));
      } else if (fl == "path" || fl == "paths" || fl == "file" || fl == "files") {
        type_filter = "path";
        text_filter = sp == std::string::npos ? "" : trim_sv(filter.substr(sp + 1));
      } else if (fl == "code" || fl == "snippet" || fl == "snippets") {
        type_filter = "code";
        text_filter = sp == std::string::npos ? "" : trim_sv(filter.substr(sp + 1));
      } else if (fl == "ip" || fl == "ips") {
        type_filter = "ip";
        text_filter = sp == std::string::npos ? "" : trim_sv(filter.substr(sp + 1));
      } else if (fl == "text" || fl == "plain") {
        type_filter = "text";
        text_filter = sp == std::string::npos ? "" : trim_sv(filter.substr(sp + 1));
      }
    }
    auto& store = ClipStore::instance();
    auto hits = text_filter.empty() ? store.texts() : store.search(text_filter, 64);
    if (!type_filter.empty()) {
      std::vector<std::string> typed;
      for (auto& t : hits) {
        if (typed.size() >= 8) break;
        auto ct = clip_type_of(t);
        if (type_filter == "text" ? ct == "text" : ct == type_filter) typed.push_back(t);
      }
      hits.swap(typed);
    } else if (!text_filter.empty() && hits.size() > 8) {
      hits.resize(8);
    }
    if (hits.empty()) {
      out.push_back(card(text_filter.empty() && type_filter.empty()
                             ? "No clipboard history yet"
                             : (type_filter.empty() ? "No clips matching"
                                                    : "No " + type_filter + " clips matching"),
                         filter.empty() ? "Copy text, then type clips" : "Try clips to list all",
                         "", "clips", 9000, ResultAction::None));
    } else {
      int n = 0;
      for (auto& t : hits) {
        bool pin = store.pinned(t);
        std::string sub = pin ? "Pinned clip · enter copies" : "Clipboard history";
        if (!type_filter.empty()) sub = type_filter + " clip · enter copies";
        SearchResult r = card(clipboard_preview(t), sub, t, "clips", 10000 - n);
        r.category = "clips";
        r.actions.push_back({"copy_text", "Copy"});
        r.actions.push_back({pin ? "clip_unpin" : "clip_pin", pin ? "Unpin" : "Pin"});
        out.push_back(std::move(r));
        if (++n >= 8) break;
      }
    }
    return out;
  }

  if (intent.kind == MiniKind::Browser) {
    if (!cfg.browser.library) return out;
    auto filter = trim_sv(intent.remainder);
    auto fl = to_lower_utf8(filter);
    std::string want_source;
    if (fl == "tabs" || fl == "tab")
      want_source = "tab";
    else if (fl == "bookmarks" || fl == "bookmark")
      want_source = "bookmark";
    else if (fl == "history" || fl == "hist")
      want_source = "history";
    auto items = browser_library(true, true);
    int n = 0;
    for (auto& it : items) {
      if (n >= 12) break;
      if (!want_source.empty()) {
        if (it.source != want_source) continue;
      } else if (!filter.empty()) {
        auto t = fold_search(it.title);
        auto u = to_lower_utf8(it.url);
        if (t.find(to_lower_utf8(filter)) == std::string::npos &&
            u.find(to_lower_utf8(filter)) == std::string::npos)
          continue;
      }
      SearchResult r = card(it.title.empty() ? it.url : it.title, it.url, it.url, it.source,
                            10000 - n * 10, ResultAction::WebSearch);
      r.category = "browser";
      out.push_back(std::move(r));
      ++n;
    }
    if (out.empty()) {
      out.push_back(card("No browser items", "No bookmarks, history, or tabs match", "", "browser",
                         9000, ResultAction::None));
    }
    return out;
  }

  if (intent.kind == MiniKind::Os) {
#if defined(_WIN32)
    const char* os = "Windows";
#elif defined(WILFRED_IOS)
    const char* os = "iOS";
#elif defined(__APPLE__)
    const char* os = "macOS";
#elif defined(__FreeBSD__)
    const char* os = "FreeBSD";
#elif defined(__OpenBSD__)
    const char* os = "OpenBSD";
#elif defined(__NetBSD__)
    const char* os = "NetBSD";
#elif defined(__DragonFly__)
    const char* os = "DragonFly BSD";
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

  if (intent.kind == MiniKind::Screenshot) {
    auto dir = native_screenshot_save_directory();
    auto shot_card = [&](ScreenshotMode m, const char* title, const char* hint, int score) {
      SearchResult r;
      r.title = title;
      std::string sub = hint + std::string(" · saves to ") + dir + " · Enter captures";
      r.subtitle = sub;
      r.payload = screenshot_payload(m);
      r.path = r.payload;
      r.action = ResultAction::Screenshot;
      r.score = score;
      r.kind_label = "screenshot";
      r.category = "screenshot";
      return r;
    };
    ScreenshotMode single = ScreenshotMode::Fullscreen;
    bool filtered = !intent.remainder.empty() && parse_screenshot_mode(intent.remainder, single);
    if (!filtered) {
      out.push_back(shot_card(ScreenshotMode::Fullscreen, "Capture fullscreen",
                             "Full screen", 10000));
      out.push_back(
          shot_card(ScreenshotMode::Window, "Capture window", "Active window", 9990));
      out.push_back(shot_card(ScreenshotMode::Region, "Capture region",
                              "Drag to select a region", 9980));
      return out;
    }
    const char* title = "Capture fullscreen";
    const char* hint = "Full screen";
    if (single == ScreenshotMode::Window) {
      title = "Capture window";
      hint = "Active window";
    } else if (single == ScreenshotMode::Region) {
      title = "Capture region";
      hint = "Drag to select a region";
    }
    out.push_back(shot_card(single, title, hint, 10000));
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
    out.push_back(
        card("Display", "Resolution unavailable here", "", "screen", 8000, ResultAction::None));
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
#elif defined(__FreeBSD__)
    // swapinfo -k: "Device  1K-blocks  Used  Avail  Capacity" per device;
    // totals are summed so multi-device swap is accounted.
    std::uint64_t total = 0, used = 0;
    if (FILE* f = popen("swapinfo -k 2>/dev/null", "r")) {
      char line[256];
      bool header = true;
      while (fgets(line, sizeof(line), f)) {
        if (header) {
          header = false;
          continue;
        }
        std::string dev;
        unsigned long t = 0, u = 0;
        std::istringstream is(line);
        if (is >> dev >> t >> u) {
          total += static_cast<std::uint64_t>(t) * 1024ull;
          used += static_cast<std::uint64_t>(u) * 1024ull;
        }
      }
      pclose(f);
    }
    if (total) {
      int pct = static_cast<int>((used * 100) / total);
      char title[128];
      std::snprintf(title, sizeof(title), "Swap  %d%%  ·  %s of %s", pct,
                    human_bytes(used).c_str(), human_bytes(total).c_str());
      out.push_back(card(title, "Swap space", title, "swap", 10000, ResultAction::Copy, pct));
    } else {
      out.push_back(card("No swap", "Swap is unused or disabled", "No swap", "swap"));
    }
#elif defined(__OpenBSD__) || defined(__NetBSD__)
    // swapctl(2): SWAP_NSWAP counts devices, SWAP_STATS fills struct
    // swapent (se_nblks/se_inuse in 512-byte blocks).
    std::uint64_t total = 0, used = 0;
    int nswap = swapctl(SWAP_NSWAP, nullptr, 0);
    if (nswap > 0) {
      std::vector<swapent> sw(static_cast<std::size_t>(nswap));
      int got = swapctl(SWAP_STATS, sw.data(), nswap);
      for (int i = 0; i < got; ++i) {
        if (!(sw[static_cast<std::size_t>(i)].se_flags & SWF_ENABLE)) continue;
        total += static_cast<std::uint64_t>(sw[static_cast<std::size_t>(i)].se_nblks) * 512ull;
        used += static_cast<std::uint64_t>(sw[static_cast<std::size_t>(i)].se_inuse) * 512ull;
      }
    }
    if (total && used <= total) {
      int pct = static_cast<int>((used * 100) / total);
      char title[128];
      std::snprintf(title, sizeof(title), "Swap  %d%%  ·  %s of %s", pct,
                    human_bytes(used).c_str(), human_bytes(total).c_str());
      out.push_back(card(title, "Swap space", title, "swap", 10000, ResultAction::Copy, pct));
    } else {
      out.push_back(card("Swap stats unavailable", "No swap devices reported", "No swap", "swap"));
    }
#elif defined(__DragonFly__)
    // DragonFly ships swapinfo(8) with -k like FreeBSD, but the column
    // layout is only best-effort here: validate before showing numbers.
    std::uint64_t total = 0, used = 0;
    if (FILE* f = popen("swapinfo -k 2>/dev/null", "r")) {
      char line[256];
      bool header = true;
      while (fgets(line, sizeof(line), f)) {
        if (header) {
          header = false;
          continue;
        }
        std::string dev;
        unsigned long t = 0, u = 0;
        std::istringstream is(line);
        if (is >> dev >> t >> u) {
          total += static_cast<std::uint64_t>(t) * 1024ull;
          used += static_cast<std::uint64_t>(u) * 1024ull;
        }
      }
      pclose(f);
    }
    if (total && used <= total) {
      int pct = static_cast<int>((used * 100) / total);
      char title[128];
      std::snprintf(title, sizeof(title), "Swap  %d%%  ·  %s of %s", pct,
                    human_bytes(used).c_str(), human_bytes(total).c_str());
      out.push_back(card(title, "Swap space", title, "swap", 10000, ResultAction::Copy, pct));
    } else {
      out.push_back(card("Swap stats unavailable", "Swap reporting needs verification here", "",
                         "swap"));
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

  if (intent.kind == MiniKind::Emoji) return glyph_results(intent.remainder, false);
  if (intent.kind == MiniKind::Symbol) return glyph_results(intent.remainder, true);

  if (intent.kind == MiniKind::Fx) {
    auto try_fx = [](const std::string& rest, MathResult& m) -> bool {
      auto t = trim_sv(rest);
      if (t.empty()) return false;
      if (convert_metric(t, m) && m.currency) return true;
      if (m.ok && m.conversion && !m.currency) return false;
      auto sp = t.find_last_of(' ');
      if (sp == std::string::npos || sp == 0 || sp + 1 >= t.size()) return false;
      auto rewritten = t.substr(0, sp) + " to " + t.substr(sp + 1);
      return convert_metric(rewritten, m) && m.currency;
    };
    MathResult m;
    if (try_fx(intent.remainder, m)) {
      out.push_back(card(m.display, "Currency · enter copies", m.display, "fx", 10000,
                         ResultAction::Convert));
      return out;
    }
    if (!intent.remainder.empty()) {
      out.push_back(card("Can't convert that", "Try 100 usd to eur  or  50 gbp jpy", "", "fx", 9000,
                         ResultAction::None));
      return out;
    }
    static const char* pairs[][2] = {{"usd", "eur"}, {"usd", "gbp"}, {"usd", "jpy"},
                                     {"eur", "usd"}, {"gbp", "usd"}, {"usd", "cad"},
                                     {"usd", "inr"}, {"usd", "aud"}};
    auto iso_up = [](const char* s) {
      std::string o(s);
      for (char& c : o)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      return o;
    };
    int n = 0;
    for (auto& p : pairs) {
      MathResult r;
      if (!convert_currency(1, p[0], p[1], r) || !r.ok) continue;
      auto title = "1 " + iso_up(p[0]) + "  →  " + r.display;
      out.push_back(card(title, "Spot rate · enter copies", r.display, "fx", 10000 - n,
                         ResultAction::Convert));
      ++n;
    }
    if (out.empty())
      out.push_back(card("Currency", "Type fx 100 usd to eur", "", "fx", 8000, ResultAction::None));
    return out;
  }

  if (intent.kind == MiniKind::Tz) {
    auto rest = trim_sv(intent.remainder);
    MathResult m;
    if (!rest.empty() && (convert_datetime(rest, m) || convert_datetime("now in " + rest, m))) {
      out.push_back(card(m.display, "Timezone · enter copies", m.display, "tz", 10000,
                         ResultAction::Convert));
      return out;
    }
    const char* cities[] = {"tokyo", "london", "new york", "los angeles", "sydney",
                            "utc",   "paris",  "mumbai",   "dubai",       nullptr};
    int n = 0;
    for (auto** p = cities; *p; ++p) {
      MathResult r;
      if (!convert_datetime(std::string("now in ") + *p, r) || !r.ok) continue;
      out.push_back(card(r.display, *p, r.display, "tz", 10000 - n, ResultAction::Convert));
      ++n;
    }
    if (out.empty())
      out.push_back(card("Timezones", "Type tz tokyo  or  3pm est to pst", "", "tz", 8000,
                         ResultAction::None));
    return out;
  }

  if (intent.kind == MiniKind::Color) {
    auto rest = trim_sv(intent.remainder);
    MathResult m;
    if (!rest.empty() && convert_color(rest, m) && m.ok) {
      out.push_back(card(m.color_hex, "Hex · enter copies", m.color_hex, "color", 10000,
                         ResultAction::Convert));
      out.push_back(card(m.color_rgb, "RGB · enter copies", m.color_rgb, "color", 9990,
                         ResultAction::Convert));
      out.push_back(card(m.color_hsl, "HSL · enter copies", m.color_hsl, "color", 9980,
                         ResultAction::Convert));
      return out;
    }
    out.push_back(
        card("Color", "Type #ff5500  or  rgb(255, 85, 0)", "", "color", 8000, ResultAction::None));
    return out;
  }

  if (intent.kind == MiniKind::Uuid) {
    MathResult m;
    if (convert_devutil("uuid", m) && m.ok)
      out.push_back(card(m.display, "UUID v4 · enter copies", m.display, "uuid", 10000,
                         ResultAction::Convert));
    return out;
  }

  if (intent.kind == MiniKind::Base64) {
    auto l = to_lower_utf8(query);
    bool decode = l.rfind("base64d", 0) == 0 || l.rfind("b64d", 0) == 0 ||
                  l.rfind("decode64", 0) == 0 || l.rfind("base64decode", 0) == 0 ||
                  l.rfind("b64decode", 0) == 0;
    auto payload = intent.remainder.empty() ? clipboard : intent.remainder;
    if (payload.empty()) {
      out.push_back(card(decode ? "Base64 decode" : "Base64 encode",
                         "Type text after the command, or copy something first", "", "base64", 8000,
                         ResultAction::None));
      return out;
    }
    MathResult m;
    auto expr = std::string(decode ? "base64d " : "base64 ") + payload;
    if (convert_devutil(expr, m) && m.ok) {
      out.push_back(card(m.display, decode ? "Base64 decode · enter copies"
                                           : "Base64 encode · enter copies",
                         m.display, "base64", 10000, ResultAction::Convert));
      if (!decode) {
        MathResult d;
        if (convert_devutil("base64d " + payload, d) && d.ok && d.display != payload)
          out.push_back(card(d.display, "Looks like Base64 · decoded", d.display, "base64", 9900,
                             ResultAction::Convert));
      }
    } else {
      out.push_back(card("Can't convert that", "Need plain text to encode, or valid Base64 to decode",
                         "", "base64", 8000, ResultAction::None));
    }
    return out;
  }

  if (intent.kind == MiniKind::Sha256) {
    auto payload = intent.remainder.empty() ? clipboard : intent.remainder;
    if (payload.empty()) {
      out.push_back(card("SHA-256", "Type sha256 <text>, or copy text first", "", "sha256", 8000,
                         ResultAction::None));
      return out;
    }
    MathResult m;
    if (convert_devutil("sha256 " + payload, m) && m.ok)
      out.push_back(card(m.display, "SHA-256 · enter copies", m.display, "sha256", 10000,
                         ResultAction::Convert));
    return out;
  }

  if (intent.kind == MiniKind::Lorem) {
    MathResult m;
    auto expr = intent.remainder.empty() ? std::string("lorem") : ("lorem " + intent.remainder);
    if (convert_devutil(expr, m) && m.ok)
      out.push_back(card(m.display, "Lorem ipsum · enter copies", m.display, "lorem", 10000,
                         ResultAction::Convert));
    return out;
  }

  if (intent.kind == MiniKind::Json) {
    auto payload = intent.remainder;
    auto pl = to_lower_utf8(payload);
    if (pl.rfind("pretty ", 0) == 0) payload = payload.substr(7);
    else if (pl == "pretty") payload.clear();
    if (payload.empty()) payload = clipboard;
    if (payload.empty()) {
      out.push_back(card("JSON pretty-print", "Type json {\"a\":1}  or copy JSON first", "", "json",
                         8000, ResultAction::None));
      return out;
    }
    MathResult pretty, compact;
    if (convert_devutil("json " + payload, pretty) && pretty.ok) {
      out.push_back(card(pretty.display, "Pretty JSON · enter copies", pretty.display, "json", 10000,
                         ResultAction::Convert));
      if (convert_devutil("json minify " + payload, compact) && compact.ok &&
          compact.display != pretty.display)
        out.push_back(card(compact.display, "Minified JSON · enter copies", compact.display, "json",
                           9900, ResultAction::Convert));
    } else {
      out.push_back(card("Invalid JSON", "Couldn't parse that object or array", "", "json", 8000,
                         ResultAction::None));
    }
    return out;
  }

  if (intent.kind == MiniKind::System) {
    auto id = intent.remainder;
    struct SysRow {
      const char* id;
      const char* title;
      const char* sub;
    };
    static const SysRow rows[] = {
        {"lock", "Lock screen", "Enter to lock this computer"},
        {"sleep", "Sleep", "Enter to suspend this computer"},
        {"shutdown", "Shut down", "Enter to power off this computer"},
        {"restart", "Restart", "Enter to reboot this computer"},
        {"logout", "Log out", "Enter to sign out of this session"},
        {"empty_trash", "Empty recycle bin", "Enter to empty the trash / recycle bin"},
        {nullptr, nullptr, nullptr},
    };
    for (auto* p = rows; p->id; ++p) {
      if (id == p->id) {
        auto r = card(p->title, p->sub, p->id, p->id, 10000, ResultAction::System);
        r.category = "system";
        out.push_back(std::move(r));
        return out;
      }
    }
    return out;
  }

  if (intent.kind == MiniKind::Timer) {
    auto rest = trim_sv(intent.remainder);
    // Detect which word invoked us (timer vs pomodoro) for defaults.
    auto ql = to_lower_utf8(query);
    bool is_pomo = ql.rfind("pomodoro", 0) == 0 || ql.rfind("pomo", 0) == 0;
    auto rl = to_lower_utf8(rest);
    if (rl == "stop" || rl == "cancel" || rl == "clear" || rl == "off") {
      auto msg = TimerStore::instance().stop("");
      out.push_back(card(msg, "Timer", msg, "timer", 10000, ResultAction::Copy));
      return out;
    }
    if (rl.rfind("stop ", 0) == 0) {
      auto msg = TimerStore::instance().stop(trim_sv(rest.substr(5)));
      out.push_back(card(msg, "Timer", msg, "timer", 10000, ResultAction::Copy));
      return out;
    }
    if (rest.empty()) {
      auto timers = TimerStore::instance().list();
      if (timers.empty()) {
        out.push_back(card(is_pomo ? "Pomodoro 25m · focus, 5m break" : "Timer",
                           is_pomo ? "Type pomodoro / pomodoro 5 / pomodoro break"
                                   : "Type timer 10m · timer stop · stopwatch",
                           "", "timer", 8000, ResultAction::None));
        return out;
      }
      auto now = unix_millis();
      int n = 0;
      for (auto& t : timers) {
        auto rem = t.ends_at_ms - now;
        std::string title = t.label + " · " + (rem <= 0 ? "done" : format_remaining_ms(rem));
        SearchResult r = card(title, "Timer · enter copies", title, "timer", 10000 - n);
        r.actions.clear();
        r.actions.push_back({"copy_text", "Copy"});
        r.actions.push_back({"timer_stop", "Stop"});
        out.push_back(std::move(r));
        ++n;
      }
      return out;
    }
    // Pomodoro presets: bare `pomodoro` = 25m, `pomodoro break` = 5m.
    std::int64_t dur = 0;
    std::string label = is_pomo ? "pomodoro" : "timer";
    if (is_pomo && (rl.empty() || rl == "start" || rl == "focus" || rl == "work")) {
      dur = 25 * 60 * 1000;
    } else if (is_pomo && (rl == "break" || rl == "rest" || rl == "short")) {
      dur = 5 * 60 * 1000;
      label = "pomodoro break";
    } else if (is_pomo && (rl == "long" || rl == "long break")) {
      dur = 15 * 60 * 1000;
      label = "pomodoro long break";
    } else if (parse_duration_ms(rest, dur)) {
      if (is_pomo) label = "pomodoro " + rest;
    } else {
      // Try "<label> <duration>"? Keep simple: fail with hint.
      out.push_back(card("Can't parse that duration",
                         "Try timer 10m · 25 · 1h30m · 5:00 · pomodoro break", "", "timer", 8000,
                         ResultAction::None));
      return out;
    }
    auto msg = TimerStore::instance().start(label, dur, is_pomo);
    SearchResult r = card(msg, (is_pomo ? "Pomodoro" : "Timer") + std::string(" · enter copies"),
                          msg, "timer", 10000, ResultAction::Copy);
    r.actions.clear();
    r.actions.push_back({"copy_text", "Copy"});
    r.actions.push_back({"timer_stop", "Stop"});
    out.push_back(std::move(r));
    return out;
  }

  if (intent.kind == MiniKind::Stopwatch) {
    auto rest = to_lower_utf8(trim_sv(intent.remainder));
    auto& sw = Stopwatch::instance();
    if (rest == "start" || rest == "go" || rest.empty()) {
      if (rest.empty() && sw.running()) {
        auto el = sw.elapsed_ms();
        out.push_back(card("Stopwatch · " + format_duration_ms(el), "Running · enter copies",
                           format_duration_ms(el), "stopwatch", 10000, ResultAction::Copy));
        auto laps = sw.laps();
        int n = 0;
        for (auto& lp : laps) out.push_back(card(lp, "Lap", lp, "stopwatch", 9900 - n++));
        return out;
      }
      auto msg = sw.start();
      out.push_back(card(msg, "Stopwatch · enter copies", msg, "stopwatch", 10000,
                         ResultAction::Copy));
      return out;
    }
    if (rest == "stop" || rest == "pause") {
      auto msg = sw.stop();
      out.push_back(card(msg, "Stopwatch · enter copies", msg, "stopwatch", 10000,
                         ResultAction::Copy));
      return out;
    }
    if (rest == "reset" || rest == "clear") {
      auto msg = sw.reset();
      out.push_back(card(msg, "Stopwatch", msg, "stopwatch", 10000, ResultAction::Copy));
      return out;
    }
    if (rest.rfind("lap", 0) == 0) {
      std::string label = trim_sv(intent.remainder.substr(3));
      auto msg = sw.lap(label);
      out.push_back(card(msg, "Stopwatch lap · enter copies", msg, "stopwatch", 10000,
                         ResultAction::Copy));
      return out;
    }
    auto el = sw.elapsed_ms();
    out.push_back(card("Stopwatch · " + format_duration_ms(el),
                       "start · stop · lap · reset", format_duration_ms(el), "stopwatch", 10000,
                       ResultAction::Copy));
    return out;
  }

  if (intent.kind == MiniKind::Note) {
    auto rest = trim_sv(intent.remainder);
    auto rl = to_lower_utf8(rest);
    auto& store = QuickNoteStore::instance();
    if (rl == "clear") {
      store.clear();
      out.push_back(card("Notes cleared", "All quick notes removed", "", "note", 10000,
                         ResultAction::Copy));
      return out;
    }
    if (rl.rfind("add ", 0) == 0) rest = trim_sv(rest.substr(4));
    if (rl.rfind("rm ", 0) == 0 || rl.rfind("del ", 0) == 0 || rl.rfind("remove ", 0) == 0) {
      auto id = trim_sv(rest.substr(rest.find(' ') + 1));
      bool ok = store.remove(id);
      out.push_back(card(ok ? "Note removed" : "No note " + id, ok ? id : "Try notes to list",
                         "", "note", 9000, ResultAction::None));
      return out;
    }
    if (!rest.empty() && rl != "list" && rl != "ls") {
      // `note <text>` adds; `notes` lists.
      auto ql = to_lower_utf8(query);
      bool list_only = ql == "notes" || ql == "note" || rl == "list" || rl == "ls";
      if (!list_only) {
        auto id = store.add(rest);
        out.push_back(card("Note saved (#" + id + ")", rest, rest, "note", 10000,
                           ResultAction::Copy));
        return out;
      }
    }
    auto notes = store.list(rl == "list" || rl == "ls" ? "" : rest, 8);
    if (notes.empty()) {
      out.push_back(card("No notes yet", "Type note <text> to save one", "", "note", 9000,
                         ResultAction::None));
      return out;
    }
    int n = 0;
    for (auto& nt : notes) {
      SearchResult r = card("#" + nt.id + " · " + clipboard_preview(nt.text), "Note · enter copies",
                            nt.text, "note", 10000 - n++);
      r.actions.clear();
      r.actions.push_back({"copy_text", "Copy"});
      r.actions.push_back({"note_delete:" + nt.id, "Delete"});
      out.push_back(std::move(r));
    }
    return out;
  }

  if (intent.kind == MiniKind::Todo) {
    auto rest = trim_sv(intent.remainder);
    auto rl = to_lower_utf8(rest);
    auto& store = TodoStore::instance();
    if (rl == "clear") {
      store.clear_all();
      out.push_back(card("Todos cleared", "All tasks removed", "", "todo", 10000,
                         ResultAction::Copy));
      return out;
    }
    if (rl == "clear done" || rl == "clear-done" || rl == "clean") {
      store.clear_done();
      out.push_back(card("Done todos cleared", "", "", "todo", 10000, ResultAction::Copy));
      return out;
    }
    if (rl.rfind("done ", 0) == 0 || rl.rfind("check ", 0) == 0) {
      auto id_s = trim_sv(rest.substr(rest.find(' ') + 1));
      try {
        int id = std::stoi(id_s);
        bool ok = store.set_done(id, true);
        out.push_back(card(ok ? "Todo #" + std::to_string(id) + " done" : "No todo " + id_s, "",
                           "", "todo", 9000, ResultAction::None));
      } catch (...) {
        out.push_back(card("Usage: todo done <id>", "Try todos to list ids", "", "todo", 8000,
                           ResultAction::None));
      }
      return out;
    }
    if (rl.rfind("undo ", 0) == 0 || rl.rfind("uncheck ", 0) == 0 || rl.rfind("open ", 0) == 0) {
      auto id_s = trim_sv(rest.substr(rest.find(' ') + 1));
      try {
        int id = std::stoi(id_s);
        bool ok = store.set_done(id, false);
        out.push_back(card(ok ? "Todo #" + std::to_string(id) + " reopened" : "No todo " + id_s,
                           "", "", "todo", 9000, ResultAction::None));
      } catch (...) {
        out.push_back(card("Usage: todo undo <id>", "", "", "todo", 8000, ResultAction::None));
      }
      return out;
    }
    if (rl.rfind("rm ", 0) == 0 || rl.rfind("del ", 0) == 0 || rl.rfind("remove ", 0) == 0) {
      auto id_s = trim_sv(rest.substr(rest.find(' ') + 1));
      try {
        int id = std::stoi(id_s);
        bool ok = store.remove(id);
        out.push_back(card(ok ? "Todo removed" : "No todo " + id_s, "", "", "todo", 9000,
                           ResultAction::None));
      } catch (...) {
        out.push_back(card("Usage: todo rm <id>", "", "", "todo", 8000, ResultAction::None));
      }
      return out;
    }
    if (rl.rfind("add ", 0) == 0) rest = trim_sv(rest.substr(4));
    if (!rest.empty()) {
      auto ql = to_lower_utf8(query);
      bool list_only = ql == "todos" || ql == "todo" || rl == "list" || rl == "ls";
      if (!list_only) {
        auto id = store.add(rest);
        out.push_back(card("Todo #" + id + " added", rest, rest, "todo", 10000,
                           ResultAction::Copy));
        return out;
      }
    }
    auto items = store.list(true, (rl == "list" || rl == "ls") ? "" : rest, 12);
    if (items.empty()) {
      out.push_back(card("No todos", "Type todo <task> to add one", "", "todo", 9000,
                         ResultAction::None));
      return out;
    }
    int n = 0;
    for (auto& t : items) {
      std::string title = (t.done ? "[x] " : "[ ] ") + std::string("#") + std::to_string(t.id) +
                          " · " + t.text;
      SearchResult r = card(title, t.done ? "Done · enter copies" : "Open · todo done " +
                                                                          std::to_string(t.id),
                            t.text, "todo", 10000 - n++);
      r.actions.clear();
      r.actions.push_back({"copy_text", "Copy"});
      r.actions.push_back({t.done ? "todo_undo:" + std::to_string(t.id)
                                  : "todo_done:" + std::to_string(t.id),
                           t.done ? "Reopen" : "Done"});
      r.actions.push_back({"todo_delete:" + std::to_string(t.id), "Delete"});
      out.push_back(std::move(r));
    }
    return out;
  }

  if (intent.kind == MiniKind::Media) {
    auto rest = trim_sv(intent.remainder);
    auto rl = to_lower_utf8(rest);
    // Normalize "play"/"pause"/etc with optional nouns.
    std::string act;
    if (rl.empty() || rl == "play" || rl == "pause" || rl == "playpause" || rl == "toggle" ||
        rl == "play pause" || rl == "play/pause")
      act = "playpause";
    else if (rl == "next" || rl == "next track" || rl == "skip" || rl == "forward")
      act = "next";
    else if (rl == "prev" || rl == "previous" || rl == "previous track" || rl == "back")
      act = "prev";
    else if (rl == "stop" || rl == "stop music")
      act = "stop";
    else if (rl == "mute" || rl == "unmute" || rl == "mute toggle")
      act = "mute";
    else if (rl == "vol up" || rl == "volume up" || rl == "up" || rl == "louder" || rl == "vol+")
      act = "volup";
    else if (rl == "vol down" || rl == "volume down" || rl == "down" || rl == "quieter" ||
             rl == "vol-")
      act = "voldn";
    else if (rl.rfind("volume ", 0) == 0 || rl.rfind("vol ", 0) == 0)
      act = "";
    if (act.empty() && !rl.empty()) {
      out.push_back(card("Media", "Try media play · pause · next · prev · mute · vol up/down", "",
                         "media", 8000, ResultAction::None));
      return out;
    }
    if (!act.empty()) {
      std::string err;
      bool ok = native_media_action(act, err);
      std::string title = ok ? ("Media · " + act) : ("Media failed · " + err);
      SearchResult r = card(title, ok ? "Sent to system player" : err, title, "media",
                            10000, ok ? ResultAction::Copy : ResultAction::None);
      r.category = "media";
      r.actions.clear();
      r.actions.push_back({"media:" + act, "Run again"});
      out.push_back(std::move(r));
      return out;
    }
    struct Row {
      const char* id;
      const char* title;
    };
    static const Row rows[] = {{"playpause", "Play / pause"},
                               {"next", "Next track"},
                               {"prev", "Previous track"},
                               {"mute", "Mute toggle"},
                               {"volup", "Volume up"},
                               {"voldn", "Volume down"},
                               {nullptr, nullptr}};
    int n = 0;
    for (auto* p = rows; p->id; ++p) {
      SearchResult r = card(p->title, "Media control · enter runs", std::string("media:") + p->id,
                            "media", 10000 - n++ * 10, ResultAction::Copy);
      r.category = "media";
      r.actions.clear();
      r.actions.push_back({std::string("media:") + p->id, "Run"});
      out.push_back(std::move(r));
    }
    return out;
  }

  if (intent.kind == MiniKind::Ping) {
    auto host = trim_sv(intent.remainder);
    if (host.empty()) {
      out.push_back(card("Ping", "Type ping <host>", "", "ping", 8000, ResultAction::None));
      return out;
    }
    auto sum = ping_summary(host);
    out.push_back(card(host + " · " + (sum.empty() ? "no response" : sum), "Ping · enter copies",
                       sum.empty() ? host : sum, "ping", 10000, ResultAction::Copy));
    return out;
  }

  if (intent.kind == MiniKind::Dns) {
    auto host = trim_sv(intent.remainder);
    // Allow `dns <host>` or `dns <host> <extra>`? Take first token.
    {
      auto sp = host.find(' ');
      if (sp != std::string::npos) host.resize(sp);
    }
    if (host.empty()) {
      out.push_back(card("DNS lookup", "Type dns <hostname>", "", "dns", 8000,
                         ResultAction::None));
      return out;
    }
    auto ips = dns_lookup(host);
    if (ips.empty()) {
      out.push_back(card("No address for " + host, "DNS lookup failed", "", "dns", 9000,
                         ResultAction::None));
      return out;
    }
    int n = 0;
    for (auto& ip : ips)
      out.push_back(card(ip, host + " · DNS · enter copies", ip, "dns", 10000 - n++ * 10,
                         ResultAction::Copy));
    return out;
  }

  if (intent.kind == MiniKind::MyIp) {
    auto ip = public_ip();
    if (ip.empty()) {
      if (!g_net)
        out.push_back(card("Public IP", "Network checks are disabled", "", "myip", 8000,
                           ResultAction::None));
      else
        out.push_back(card("Public IP unavailable", "Offline or blocked", "", "myip", 8000,
                           ResultAction::None));
      return out;
    }
    out.push_back(card(ip, "Public IP · enter copies", ip, "myip", 10000, ResultAction::Copy));
    auto local = first_ipv4();
    if (!local.empty() && local != ip)
      out.push_back(card(local, "Local IPv4 · enter copies", local, "myip", 9900,
                         ResultAction::Copy));
    return out;
  }

  if (intent.kind == MiniKind::Base || intent.kind == MiniKind::Bits ||
      intent.kind == MiniKind::Regex || intent.kind == MiniKind::UrlCodec ||
      intent.kind == MiniKind::Jwt) {
    // Delegate to the calculator/dev utilities so `hex`, `bit`, `regex`, `url`,
    // `jwt` work with or without extra words, using clipboard as fallback.
    std::string expr = query;
    // `base`/`bit` with no payload: show usage, not an error card that blocks.
    auto rem = trim_sv(intent.remainder);
    if (rem.empty()) {
      if (intent.kind == MiniKind::Base)
        out.push_back(card("Number bases", "Try hex 255 · dec 0xff · bin 10 · base 16 255",
                           "", "base", 8000, ResultAction::None));
      else if (intent.kind == MiniKind::Bits)
        out.push_back(card("Bit tools", "Try bit and 12 10 · bit or 12 10 · bit not 5",
                           "", "bits", 8000, ResultAction::None));
      else if (intent.kind == MiniKind::Regex)
        out.push_back(card("Regex tester", "Try regex foo.* \"foobar\" · regexi hi HELLO", "",
                           "regex", 8000, ResultAction::None));
      else if (intent.kind == MiniKind::UrlCodec)
        out.push_back(card("URL codec", "Try urlencode a b&c · urldecode a%20b", "", "url", 8000,
                           ResultAction::None));
      else
        out.push_back(card("JWT decode", "Paste a header.payload.signature token", "", "jwt",
                           8000, ResultAction::None));
      // If clipboard can complete it, also show the converted clipboard.
      if (!clipboard.empty()) {
        MathResult m;
        if (convert_devutil(expr + " " + clipboard, m) && m.ok)
          out.push_back(card(m.display, "From clipboard · enter copies", m.display, "dev", 9900,
                             ResultAction::Convert));
      }
      return out;
    }
    MathResult m;
    if (convert_devutil(expr, m) && m.ok) {
      out.push_back(card(m.display, "Dev · enter copies", m.display, "dev", 10000,
                         ResultAction::Convert));
      // For base overview, also offer each radix as its own copyable card.
      if (intent.kind == MiniKind::Base && m.display.find('=') != std::string::npos) {
        // display is "255 = 0xFF = 0b... = 0o..." — split for convenience.
        std::string d = m.display;
        std::size_t pos = d.find('=');
        if (pos != std::string::npos) {
          // Keep single card; splitting risks noise. Intentionally single.
        }
      }
      return out;
    }
    out.push_back(card("Can't convert that", "Check the syntax · type help for examples", "",
                       "dev", 8000, ResultAction::None));
    return out;
  }

  if (intent.kind == MiniKind::Dupes || intent.kind == MiniKind::Large) {
    auto filter = trim_sv(intent.remainder);
    // Strip optional keywords: `dupes in <dir>`, `large 10 in <dir>`, `large <dir>`.
    int limit = 8;
    std::string dir = filter;
    {
      auto l = to_lower_utf8(filter);
      // `large 20 foo` → limit 20, dir foo.
      std::string first, rest2;
      auto sp = filter.find(' ');
      if (sp != std::string::npos) {
        first = filter.substr(0, sp);
        rest2 = trim_sv(filter.substr(sp + 1));
      } else {
        first = filter;
      }
      bool first_is_num = !first.empty();
      for (char c : first)
        if (!std::isdigit(static_cast<unsigned char>(c))) first_is_num = false;
      if (first_is_num && !first.empty()) {
        try {
          limit = std::stoi(first);
        } catch (...) {
          limit = 8;
        }
        if (limit < 1) limit = 1;
        if (limit > 25) limit = 25;
        dir = rest2;
      }
      auto il = to_lower_utf8(dir);
      if (il.rfind("in ", 0) == 0) dir = trim_sv(dir.substr(3));
    }
    if (!index) {
      out.push_back(card(intent.kind == MiniKind::Dupes ? "Duplicates need the index"
                                                        : "Large files need the index",
                         "Open Wilfred with an index, or type wilfred index", "", "disk", 8000,
                         ResultAction::None));
      return out;
    }
    Config cfg2 = cfg;
    if (intent.kind == MiniKind::Dupes)
      return dupe_file_results(*index, cfg2, dir, limit);
    return large_file_results(*index, cfg2, dir, limit);
  }

  if (intent.kind == MiniKind::Workflow) {
    auto rest = trim_sv(intent.remainder);
    // `run` doubles as the shell-command prefix (`run ipconfig`); only claim it
    // when the remainder names a configured workflow.
    auto ql = to_lower_utf8(query);
    if (ql.rfind("run ", 0) == 0) {
      std::string nm;
      auto sp = rest.find(' ');
      nm = to_lower_utf8(sp == std::string::npos ? rest : rest.substr(0, sp));
      if (cfg.workflows.find(nm) == cfg.workflows.end()) return out;
      return workflow_results(nm, cfg);
    }
    if (rest.empty()) return workflow_results("", cfg);
    std::string nm;
    {
      auto sp = rest.find(' ');
      nm = to_lower_utf8(sp == std::string::npos ? rest : rest.substr(0, sp));
    }
    auto it = cfg.workflows.find(nm);
    if (it == cfg.workflows.end()) {
      // `workflows foo` with unknown name → list with hint.
      auto all = workflow_results("", cfg);
      out.insert(out.end(), all.begin(), all.end());
      return out;
    }
    return workflow_results(nm, cfg);
  }

  if (intent.kind == MiniKind::Quicklink) {
    auto m = match_quicklink(query, cfg);
    if (!m.matched) {
      if (cfg.quicklinks.empty())
        out.push_back(card("No quicklinks yet",
                           "Add quicklinks: in wilfred.yml with {query} or {1} placeholders", "",
                           "quicklink", 8000, ResultAction::None));
      else
        out.push_back(card("Quicklink", "Type ql <name> <args> · try ql to list", "",
                           "quicklink", 8000, ResultAction::None));
      // List configured ones for discovery.
      int n = 0;
      for (auto& [name, tmpl] : cfg.quicklinks) {
        if (n++ >= 8) break;
        SearchResult r = card("Quicklink · " + name, tmpl, "ql " + name + " ", "quicklink",
                              9000 - n, ResultAction::Habit);
        out.push_back(std::move(r));
      }
      return out;
    }
    return quicklink_results(m, clipboard);
  }

  if (intent.kind == MiniKind::Transcribe) {
    if (!cfg.transcription.enabled) {
      out.push_back(card("Transcription disabled",
                         "Set transcription.enabled: true in wilfred.yml", "", "transcribe", 8000,
                         ResultAction::None));
      return out;
    }
    // Setup state first: without a whisper binary/model there is nothing to run.
    auto binary = resolve_whisper_binary(cfg);
    auto model = binary.empty() ? std::string() : resolve_whisper_model(cfg);
    if (binary.empty() || model.empty()) {
      std::string what = binary.empty() ? transcribe_install_hint("whisper")
                                        : transcribe_install_hint("model");
      out.push_back(card(binary.empty() ? "Whisper not found" : "No whisper model", what, "",
                         "transcribe", 9000, ResultAction::None));
      return out;
    }
    auto target = trim_sv(intent.remainder);
    if (target.size() >= 2 &&
        ((target.front() == '"' && target.back() == '"') ||
         (target.front() == '\'' && target.back() == '\'')))
      target = target.substr(1, target.size() - 2);
    if (target.empty()) {
      out.push_back(card("Transcribe audio to text",
                         "Type transcribe <file.mp3|file.mp4> · enter transcribes", "", "transcribe",
                         8000, ResultAction::None));
      return out;
    }
    std::vector<std::string> paths;
    if (file_exists(target) && is_transcribe_candidate(target)) {
      paths.push_back(target);
    } else if (index) {
      paths = find_audio_in_index(*index, target, 8);
    }
    if (paths.empty()) {
      if (file_exists(target))
        out.push_back(card("Not an audio file",
                           "Wilfred transcribes mp3, wav, m4a, mp4, ogg, flac, opus, webm, aac, wma",
                           "", "transcribe", 8000, ResultAction::None));
      else
        out.push_back(card("No audio found", "Try an absolute path or an indexed filename", "",
                           "transcribe", 8000, ResultAction::None));
      return out;
    }
    std::string dest_note =
        cfg.transcription.save_txt ? " · transcript to clipboard + .txt" : " · transcript to clipboard";
    int n = 0;
    for (auto& p : paths) {
      SearchResult r =
          card("Transcribe " + path_filename(p), "Enter transcribes" + dest_note, p, "transcribe",
               10000 - n * 10, ResultAction::Copy);
      r.category = "transcribe";
      r.actions.clear();
      r.actions.push_back({"transcribe_run", "Transcribe"});
      r.actions.push_back({"copy_path", "Copy path"});
      out.push_back(std::move(r));
      ++n;
    }
    return out;
  }

  if (intent.kind == MiniKind::Dictate) {
    if (!cfg.transcription.enabled) {
      out.push_back(card("Dictation disabled",
                         "Set transcription.enabled: true in wilfred.yml", "", "dictate", 8000,
                         ResultAction::None));
      return out;
    }
    int seconds = 10;
    auto rest = trim_sv(intent.remainder);
    if (!rest.empty()) {
      // Bare number of seconds; anything else is usage.
      bool numeric = true;
      for (char c : rest)
        if (!std::isdigit(static_cast<unsigned char>(c))) numeric = false;
      if (!numeric) {
        out.push_back(card("Dictate from the microphone",
                           "Type dictate [seconds] · enter records, transcript goes to clipboard",
                           "", "dictate", 8000, ResultAction::None));
        return out;
      }
      try {
        seconds = std::stoi(rest);
      } catch (...) {
        seconds = 10;
      }
      if (seconds < 1) seconds = 1;
      if (seconds > 120) seconds = 120;
    }
    if (!ffmpeg_available()) {
      out.push_back(card("Microphone needs ffmpeg", transcribe_install_hint("ffmpeg"), "",
                         "dictate", 9000, ResultAction::None));
      return out;
    }
    auto binary = resolve_whisper_binary(cfg);
    auto model = binary.empty() ? std::string() : resolve_whisper_model(cfg);
    if (binary.empty() || model.empty()) {
      out.push_back(card(binary.empty() ? "Whisper not found" : "No whisper model",
                         binary.empty() ? transcribe_install_hint("whisper")
                                        : transcribe_install_hint("model"),
                         "", "dictate", 9000, ResultAction::None));
      return out;
    }
    SearchResult r = card("Dictate " + std::to_string(seconds) + "s",
                          "Enter records the microphone · transcript to clipboard",
                          std::to_string(seconds), "dictate", 10000, ResultAction::Copy);
    r.category = "dictate";
    r.actions.clear();
    r.actions.push_back({"dictate_run:" + std::to_string(seconds), "Dictate"});
    out.push_back(std::move(r));
    return out;
  }

  if (intent.kind == MiniKind::Layout) {
    auto rest = trim_sv(intent.remainder);
    auto rl = to_lower_utf8(rest);
    auto& store = LayoutStore::instance();
    auto list_card = [&](const std::string& name, int entries, int score) {
      SearchResult r = card("Layout · " + name,
                            std::to_string(entries) + " windows · enter applies", "layout_apply:" + name,
                            "layout", score, ResultAction::Copy);
      r.category = "layout";
      r.actions.clear();
      r.actions.push_back({"layout_apply:" + name, "Apply"});
      return r;
    };
    if (rest.empty() || rl == "list" || rl == "ls" || rl == "layouts") {
      auto names = store.list();
      if (names.empty()) {
        out.push_back(card("No saved layouts",
                           "Type layout save <name> to capture open windows", "", "layout", 8000,
                           ResultAction::None));
        return out;
      }
      int n = 0;
      for (auto& name : names) {
        Layout lay;
        std::string err;
        if (!store.load_layout(name, lay, err)) continue;
        out.push_back(list_card(name, static_cast<int>(lay.entries.size()), 10000 - n * 10));
        ++n;
        if (n >= 8) break;
      }
      return out;
    }
    if (rl == "save" || rl.rfind("save ", 0) == 0) {
      auto name = trim_sv(rest.size() > 4 ? rest.substr(4) : "");
      if (!LayoutStore::valid_name(name)) {
        out.push_back(card("Usage: layout save <name>",
                           "Names use letters, digits, _ and -", "", "layout", 8000,
                           ResultAction::None));
        return out;
      }
      Layout lay;
      lay.name = to_lower_utf8(name);
      for (auto& w : native_list_windows()) {
        NativeWindowRect rc;
        std::string err;
        LayoutEntry e;
        e.match = w.title.empty() ? w.owner : w.title;
        if (native_window_rect(w.id, rc, err)) {
          e.x = rc.x;
          e.y = rc.y;
          e.w = rc.w;
          e.h = rc.h;
          e.maximized = rc.maximized;
        }
        lay.entries.push_back(std::move(e));
        if (lay.entries.size() >= 64) break;
      }
      std::string err;
      if (lay.entries.empty() || !store.save_layout(lay, err)) {
        out.push_back(card(lay.entries.empty() ? "No open windows to save" : "Could not save layout",
                           err, "", "layout", 8000, ResultAction::None));
        return out;
      }
      out.push_back(card("Layout " + lay.name + " saved",
                         std::to_string(lay.entries.size()) + " windows · type layout " + lay.name +
                             " to apply",
                         "layout_apply:" + lay.name, "layout", 10000, ResultAction::Copy));
      return out;
    }
    if (rl == "delete" || rl.rfind("delete ", 0) == 0 || rl.rfind("rm ", 0) == 0 ||
        rl.rfind("remove ", 0) == 0 || rl.rfind("del ", 0) == 0) {
      auto sp = rest.find(' ');
      auto name = sp == std::string::npos ? std::string() : to_lower_utf8(trim_sv(rest.substr(sp + 1)));
      if (store.delete_layout(name))
        out.push_back(card("Layout " + name + " deleted", "", "", "layout", 10000,
                           ResultAction::Copy));
      else
        out.push_back(card("No layout " + name, "Try layouts to list", "", "layout", 8000,
                           ResultAction::None));
      return out;
    }
    {
      Layout lay;
      std::string err;
      if (!store.load_layout(to_lower_utf8(rest), lay, err)) {
        out.push_back(card("No layout " + rest, "Try layouts to list, or layout save <name>", "",
                           "layout", 8000, ResultAction::None));
        return out;
      }
      out.push_back(list_card(lay.name, static_cast<int>(lay.entries.size()), 10000));
      return out;
    }
  }

  if (intent.kind == MiniKind::Help) {
    static const char* lines[] = {"weather [city]  ·  local forecast",
                                  "time [zone]  ·  clock and date",
                                  "timer 10m · pomodoro · stopwatch  ·  focus timers",
                                  "note <text> · notes  ·  quick notes",
                                  "todo <task> · todos · todo done <id>  ·  tasks",
                                  "tz tokyo  ·  world clock / zone convert",
                                  "color #ff5500  ·  hex / rgb / hsl",
                                  "hex 255 · dec 0xff · base 16 255  ·  number bases",
                                  "bit and 12 10 · bit not 5  ·  bit tools",
                                  "regex pattern text · urlencode · jwt <token>  ·  text tools",
                                  "json {\"a\":1} · base64 · sha256 · uuid · lorem",
                                  "disk / disku  ·  drive space",
                                  "large [n] [dir] · dupes [dir]  ·  big + duplicate files",
                                  "ram / cpu / swap  ·  memory and load",
                                  "process <name> · kill <pid|name>  ·  processes",
                                  "media play · next · mute · vol up  ·  playback",
                                  "ping <host> · dns <host> · myip  ·  network",
                                  "transcribe <file>  ·  mp3/mp4 audio to text",
                                  "dictate [seconds]  ·  mic to text via whisper",
                                  "layout save <name> · layout <name>  ·  window layouts",
                                  "windows [name]  ·  switch to an open window",
                                  "minimize/maximize <name> · close window <name>",
                                  "screenshot [fullscreen|window|region]",
                                  "emoji [name]  ·  emoji picker",
                                  "symbol [name]  ·  punctuation and signs",
                                  "fx 100 usd to eur  ·  currency conversion",
                                  "lock / sleep / shutdown / restart / logout",
                                  "empty trash  ·  recycle bin",
                                  "speedtest  ·  live download and upload",
                                  "battery / ip / hostname / uptime / user",
                                  "clip / clips [type] [query]  ·  clips url · clips code",
                                  "bm [query]  ·  bookmarks, history, open tabs",
                                  "workflow <name> · run <name>  ·  multi-step actions",
                                  "ql <name> <args>  ·  parameterized quicklinks",
                                  "snip / ;keyword  ·  text snippets",
                                  "snip save <name>  ·  save clipboard as snippet",
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
