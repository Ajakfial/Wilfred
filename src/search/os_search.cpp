#include "wilfred/search/os_search.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/classify.hpp"
#include "wilfred/platform/platform.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
// Windows Search native API (ISearchManager -> catalog -> query helper).
// ole32/oleaut32 are already linked on Windows (see CMakeLists).
#include <searchapi.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace wilfred {
namespace {

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
  if (exe.empty()) return false;
#ifdef _WIN32
  return std::system(("where " + exe + " >NUL 2>&1").c_str()) == 0;
#elif defined(__ANDROID__) || defined(WILFRED_IOS)
  return false;
#else
  return std::system(("command -v " + exe + " >/dev/null 2>&1").c_str()) == 0;
#endif
}

// 60s TTL so per-keystroke provider queries stay cheap (same idea as pkg).
[[maybe_unused]] bool have_tool_cached(const std::string& exe) {
  static std::mutex mu;
  static std::map<std::string, std::pair<bool, std::chrono::steady_clock::time_point>> cache;
  std::lock_guard<std::mutex> lock(mu);
  auto now = std::chrono::steady_clock::now();
  auto it = cache.find(exe);
  if (it == cache.end() ||
      std::chrono::duration_cast<std::chrono::seconds>(now - it->second.second).count() > 60) {
    cache[exe] = {have_tool(exe), now};
  }
  return cache[exe].first;
}

// Run argv with a hard timeout; stdout captured up to max_bytes (stderr
// discarded on POSIX, merged on Windows so filters must ignore noise).
// Returns exit code, -1 on spawn failure, -2 on timeout (child killed).
[[maybe_unused]] int os_run_capture(const std::vector<std::string>& argv, int timeout_ms,
                                    std::size_t max_bytes, std::string& out) {
  out.clear();
  if (argv.empty() || timeout_ms < 500) return -1;
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

bool looks_like_posix_path(const std::string& s) {
  return s.size() > 1 && s[0] == '/';
}

bool looks_like_windows_path(const std::string& s) {
  if (s.size() >= 3 && std::isalpha(static_cast<unsigned char>(s[0])) && s[1] == ':' &&
      (s[2] == '\\' || s[2] == '/'))
    return true;
  if (s.size() >= 2 && s[0] == '\\' && s[1] == '\\') return true;
  return false;
}

bool looks_like_path(const std::string& s) {
  return looks_like_posix_path(s) || looks_like_windows_path(s);
}

std::string url_decode(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size() && std::isxdigit(static_cast<unsigned char>(s[i + 1])) &&
        std::isxdigit(static_cast<unsigned char>(s[i + 2]))) {
      auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        return c - 'A' + 10;
      };
      o.push_back(static_cast<char>(hex(s[i + 1]) * 16 + hex(s[i + 2])));
      i += 2;
    } else {
      o.push_back(s[i]);
    }
  }
  return o;
}

// tracker3 prints file:// URIs; older tracker and baloosearch print paths.
std::string strip_file_uri(const std::string& line) {
  auto pos = line.find("file://");
  if (pos == std::string::npos) return {};
  std::string uri = line.substr(pos);
  auto end = uri.find_first_of(" \t\r\n\"'");
  if (end != std::string::npos) uri.resize(end);
  std::string path = uri.substr(7);  // strip file://
  if (path.rfind("localhost", 0) == 0) path.erase(0, 9);
  path = url_decode(path);
  if (!looks_like_posix_path(path)) return {};
  return path;
}

std::vector<std::string> split_lines(const std::string& out) {
  std::vector<std::string> lines;
  std::string cur;
  for (char c : out) {
    if (c == '\n') {
      lines.push_back(cur);
      cur.clear();
    } else if (c != '\r') {
      cur.push_back(c);
      if (cur.size() > 4096) {
        lines.push_back(cur);
        cur.clear();
      }
    }
  }
  if (!cur.empty()) lines.push_back(cur);
  return lines;
}

// UTF-8 -> UTF-16LE for powershell -EncodedCommand (handles the full
// Unicode range, including surrogate pairs, so CJK queries survive).
std::string utf8_to_utf16le(const std::string& s) {
  std::string o;
  o.reserve(s.size() * 2);
  auto push = [&](std::uint32_t cp) {
    if (cp < 0x10000) {
      if (cp >= 0xD800 && cp <= 0xDFFF) cp = 0xFFFD;
      o.push_back(static_cast<char>(cp & 0xFF));
      o.push_back(static_cast<char>((cp >> 8) & 0xFF));
    } else if (cp <= 0x10FFFF) {
      cp -= 0x10000;
      std::uint32_t hi = 0xD800 + (cp >> 10);
      std::uint32_t lo = 0xDC00 + (cp & 0x3FF);
      o.push_back(static_cast<char>(hi & 0xFF));
      o.push_back(static_cast<char>((hi >> 8) & 0xFF));
      o.push_back(static_cast<char>(lo & 0xFF));
      o.push_back(static_cast<char>((lo >> 8) & 0xFF));
    } else {
      o.push_back(static_cast<char>(0xFD));
      o.push_back(static_cast<char>(0xFF));
    }
  };
  for (std::size_t i = 0; i < s.size();) {
    unsigned char c = static_cast<unsigned char>(s[i]);
    if (c < 0x80) {
      push(c);
      ++i;
    } else if ((c >> 5) == 0x6 && i + 1 < s.size()) {
      std::uint32_t cp = ((c & 0x1F) << 6) | (static_cast<unsigned char>(s[i + 1]) & 0x3F);
      push(cp);
      i += 2;
    } else if ((c >> 4) == 0xE && i + 2 < s.size()) {
      std::uint32_t cp = ((c & 0x0F) << 12) | ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                         (static_cast<unsigned char>(s[i + 2]) & 0x3F);
      push(cp);
      i += 3;
    } else if ((c >> 3) == 0x1E && i + 3 < s.size()) {
      std::uint32_t cp = ((c & 0x07) << 18) |
                         ((static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                         ((static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                         (static_cast<unsigned char>(s[i + 3]) & 0x3F);
      push(cp);
      i += 4;
    } else {
      push(0xFFFD);
      ++i;
    }
  }
  return o;
}

std::string base64_encode(const std::string& bytes) {
  static const char* k = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string o;
  o.reserve((bytes.size() + 2) / 3 * 4);
  for (std::size_t i = 0; i < bytes.size(); i += 3) {
    std::uint32_t n = static_cast<unsigned char>(bytes[i]) << 16;
    int rem = 1;
    if (i + 1 < bytes.size()) {
      n |= static_cast<unsigned char>(bytes[i + 1]) << 8;
      rem = 2;
    }
    if (i + 2 < bytes.size()) {
      n |= static_cast<unsigned char>(bytes[i + 2]);
      rem = 3;
    }
    o.push_back(k[(n >> 18) & 63]);
    o.push_back(k[(n >> 12) & 63]);
    o.push_back(rem >= 2 ? k[(n >> 6) & 63] : '=');
    o.push_back(rem >= 3 ? k[n & 63] : '=');
  }
  return o;
}

// One CONTAINS token: file-name characters only, so the SQL and the
// PowerShell wrapper cannot be broken out of (no quotes/wildcards).
std::string sanitize_token(std::string s) {
  std::string o;
  for (unsigned char c : s) {
    char ch = static_cast<char>(c);
    bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
              ch == '-' || ch == '_' || ch == '.';
    if (ok) o.push_back(ch);
    // Non-ASCII UTF-8 bytes pass through so CJK queries still match;
    // SQL string quotes are never produced from them.
    else if (c >= 0x80)
      o.push_back(ch);
    if (o.size() >= 64) break;
  }
  return o;
}

// PowerShell script querying SystemIndex via ADODB. The SQL's double quotes
// are doubled for the PS double-quoted string; the whole script travels as
// -EncodedCommand so no shell layer can mangle it.
std::string windows_ps_script(const std::string& query, int top) {
  std::string sql = os_search_windows_sql(query, top);
  if (sql.empty()) return {};
  std::string ps_sql;
  for (char c : sql) {
    if (c == '"')
      ps_sql += "\"\"";
    else
      ps_sql.push_back(c);
  }
  return "$ErrorActionPreference='SilentlyContinue';"
         "$conn=New-Object -ComObject ADODB.Connection;"
         "$conn.Open(\"Provider=Search.CollatorDSO;Extended Properties='Application=Windows';\");"
         "$rs=New-Object -ComObject ADODB.Recordset;"
         "$rs.Open(\"" +
         ps_sql +
         "\",$conn);"
         "while(!$rs.EOF){$rs.Fields.Item(\"System.ItemPathDisplay\").Value;$rs.MoveNext();}"
         "$rs.Close();$conn.Close();";
}

}  // namespace

#ifdef _WIN32
namespace {

// Minimal COM RAII (no ATL dependency).
template <typename T>
struct ComHolder {
  T* p{nullptr};
  ComHolder() = default;
  ~ComHolder() { reset(); }
  void reset(T* q = nullptr) {
    if (p) p->Release();
    p = q;
  }
  T* get() const { return p; }
  T** out() {
    reset();
    return &p;
  }
  ComHolder(const ComHolder&) = delete;
  ComHolder& operator=(const ComHolder&) = delete;
};

bool dispatch_call(IDispatch* d, const wchar_t* name, WORD flags, VARIANT* result, VARIANT* args,
                   unsigned nargs) {
  if (!d) return false;
  DISPID id = 0;
  LPOLESTR nm = const_cast<LPOLESTR>(name);
  if (FAILED(d->GetIDsOfNames(IID_NULL, &nm, 1, LOCALE_USER_DEFAULT, &id))) return false;
  DISPPARAMS dp{};
  dp.cArgs = nargs;
  dp.rgvarg = args;
  VARIANT tmp;
  VariantInit(&tmp);
  HRESULT hr = d->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, flags, &dp, result ? result : &tmp,
                         nullptr, nullptr);
  if (!result) VariantClear(&tmp);
  return SUCCEEDED(hr);
}

bool dispatch_string_prop(IDispatch* d, const wchar_t* name, std::wstring& out) {
  VARIANT v;
  VariantInit(&v);
  if (!dispatch_call(d, name, DISPATCH_PROPERTYGET, &v, nullptr, 0)) {
    VariantClear(&v);
    return false;
  }
  bool ok = false;
  if (v.vt == VT_BSTR && v.bstrVal) {
    out.assign(v.bstrVal, SysStringLen(v.bstrVal));
    ok = true;
  }
  VariantClear(&v);
  return ok;
}

// Executes SystemIndex SQL via late-bound ADO (no #import, no new link
// deps). Returns true once the recordset opened; rows are best-effort.
bool ado_run_sql(const wchar_t* sql, std::size_t cap, std::vector<std::string>& out) {
  CLSID cls{};
  ComHolder<IDispatch> conn;
  if (FAILED(CLSIDFromProgID(L"ADODB.Connection", &cls))) return false;
  if (FAILED(CoCreateInstance(cls, nullptr, CLSCTX_INPROC_SERVER, IID_IDispatch,
                              reinterpret_cast<void**>(conn.out()))))
    return false;
  VARIANT arg;
  VariantInit(&arg);
  arg.vt = VT_BSTR;
  arg.bstrVal =
      SysAllocString(L"Provider=Search.CollatorDSO;Extended Properties='Application=Windows';");
  bool opened =
      arg.bstrVal && dispatch_call(conn.get(), L"Open", DISPATCH_METHOD, nullptr, &arg, 1);
  VariantClear(&arg);
  if (!opened) return false;

  ComHolder<IDispatch> rs;
  if (FAILED(CLSIDFromProgID(L"ADODB.Recordset", &cls))) return false;
  if (FAILED(CoCreateInstance(cls, nullptr, CLSCTX_INPROC_SERVER, IID_IDispatch,
                              reinterpret_cast<void**>(rs.out()))))
    return false;
  // Recordset.Open(Source, ActiveConnection, CursorType, LockType, Options);
  // args reversed: adCmdText=1, adLockReadOnly=1, adOpenForwardOnly=0.
  VARIANT av[5];
  for (auto& v : av)
    VariantInit(&v);
  av[4].vt = VT_BSTR;
  av[4].bstrVal = SysAllocString(sql);
  av[3].vt = VT_DISPATCH;
  av[3].pdispVal = conn.get();
  if (av[3].pdispVal) av[3].pdispVal->AddRef();
  av[2].vt = VT_I4;
  av[2].lVal = 0;
  av[1].vt = VT_I4;
  av[1].lVal = 1;
  av[0].vt = VT_I4;
  av[0].lVal = 1;
  bool ok = av[4].bstrVal && dispatch_call(rs.get(), L"Open", DISPATCH_METHOD, nullptr, av, 5);
  for (auto& v : av)
    VariantClear(&v);
  if (!ok) return false;

  while (out.size() < cap) {
    VARIANT eof;
    VariantInit(&eof);
    bool eof_ok = dispatch_call(rs.get(), L"EOF", DISPATCH_PROPERTYGET, &eof, nullptr, 0);
    bool done = !eof_ok || (eof.vt == VT_BOOL && eof.boolVal != VARIANT_FALSE);
    VariantClear(&eof);
    if (done) break;
    VARIANT fields;
    VariantInit(&fields);
    if (!dispatch_call(rs.get(), L"Fields", DISPATCH_PROPERTYGET, &fields, nullptr, 0) ||
        fields.vt != VT_DISPATCH || !fields.pdispVal) {
      VariantClear(&fields);
      break;
    }
    VARIANT name;
    VariantInit(&name);
    name.vt = VT_BSTR;
    name.bstrVal = SysAllocString(L"System.ItemPathDisplay");
    VARIANT field;
    VariantInit(&field);
    bool got =
        name.bstrVal && dispatch_call(fields.pdispVal, L"Item", DISPATCH_METHOD, &field, &name, 1);
    VariantClear(&name);
    VariantClear(&fields);
    if (got && field.vt == VT_DISPATCH && field.pdispVal) {
      std::wstring ws;
      if (dispatch_string_prop(field.pdispVal, L"Value", ws) && !ws.empty())
        out.push_back(wide_to_utf8(ws));
    }
    VariantClear(&field);
    if (!dispatch_call(rs.get(), L"MoveNext", DISPATCH_METHOD, nullptr, nullptr, 0)) break;
  }
  dispatch_call(rs.get(), L"Close", DISPATCH_METHOD, nullptr, nullptr, 0);
  dispatch_call(conn.get(), L"Close", DISPATCH_METHOD, nullptr, nullptr, 0);
  return true;
}

}  // namespace

// Native SystemIndex query: ISearchQueryHelper builds locale-correct SQL
// from the raw user text (quoting/escaping handled per locale, better than
// hand-built CONTAINS), executed in-process via late-bound ADO. Returns
// true when the index answered (hits may be empty); false when SystemIndex
// is unavailable so callers can fall back to Everything CLI / PowerShell.
bool os_windows_search_com(const std::string& query, int limit, std::vector<std::string>& out) {
  out.clear();
  std::string q = trim_copy(query);
  if (q.empty() || q.size() > 256) return false;
  if (limit < 1) limit = 1;
  if (limit > 20) limit = 20;

  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool uninit = SUCCEEDED(hr) && hr != S_FALSE;
  bool ok = false;
  if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
    ComHolder<ISearchManager> mgr;
    // CSearchManager lives out-of-proc (Search service surrogate AppID),
    // so INPROC alone is never enough: ask for any server.
    if (SUCCEEDED(
            CoCreateInstance(CLSID_CSearchManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(mgr.out())))) {
      ComHolder<ISearchCatalogManager> cat;
      if (SUCCEEDED(mgr.get()->GetCatalog(L"SystemIndex", cat.out()))) {
        ComHolder<ISearchQueryHelper> helper;
        if (SUCCEEDED(cat.get()->GetQueryHelper(helper.out())) &&
            SUCCEEDED(helper.get()->put_QuerySelectColumns(L"System.ItemPathDisplay")) &&
            SUCCEEDED(helper.get()->put_QueryMaxResults(limit))) {
          auto wq = utf8_to_wide(q);
          LPWSTR sql = nullptr;
          if (SUCCEEDED(helper.get()->GenerateSQLFromUserQuery(wq.c_str(), &sql)) && sql) {
            ok = ado_run_sql(sql, static_cast<std::size_t>(limit), out);
            CoTaskMemFree(sql);
          }
        }
      }
    }
  }
  if (uninit) CoUninitialize();
  return ok;
}
#endif

bool os_search_available() {
#if defined(__ANDROID__) || defined(WILFRED_IOS)
  return false;
#else
  return true;
#endif
}

std::string os_search_default_backend() {
#if defined(__ANDROID__) || defined(WILFRED_IOS)
  return {};
#elif defined(_WIN32)
  return "windows_search";
#elif defined(__APPLE__)
  return "spotlight";
#else
  return "tracker";
#endif
}

std::string os_search_backend_label(const std::string& backend) {
  auto b = os_search_normalize_backend(backend);
  if (b == "spotlight") return "Spotlight";
  if (b == "windows_search") return "Windows Search";
  if (b == "tracker") return "Tracker";
  if (b == "baloo") return "Baloo";
  if (b == "locate") return "locate";
  if (b == "everything") return "Everything";
  return "OS index";
}

std::vector<std::string> os_search_backends() {
  return {"auto", "spotlight", "windows_search", "tracker", "baloo", "locate", "everything"};
}

std::string os_search_normalize_backend(const std::string& backend) {
  auto b = to_lower_utf8(trim_copy(backend));
  for (char& c : b)
    if (c == '-') c = '_';
  if (b == "mdfind") return "spotlight";
  if (b == "windows_search" || b == "windowssearch" || b == "win_search" || b == "systemindex" ||
      b == "system_index")
    return "windows_search";
  if (b == "tracker3" || b == "tracker_3") return "tracker";
  if (b == "baloosearch" || b == "baloosearcher") return "baloo";
  if (b == "plocate" || b == "mlocate" || b == "bsd_locate") return "locate";
  if (b == "es" || b == "es_exe" || b == "everything_search" || b == "voidtools")
    return "everything";
  return b;
}

bool os_search_should_query(const std::string& text) {
  if (text.size() > 256) return false;
  for (unsigned char c : text) {
    if (c < 0x20 && c != ' ' && c != '\t') return false;
  }
  auto needle = normalize_query(text);
  if (needle.size() < 2) return false;
  int content = 0;
  for (unsigned char c : needle) {
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 0x80) ++content;
  }
  return content >= 2;
}

bool os_search_strip_prefix(const std::string& text, std::string& remainder) {
  std::size_t i = 0;
  while (i < text.size() && (text[i] == ' ' || text[i] == '\t'))
    ++i;
  if (i + 2 >= text.size()) return false;
  if (std::tolower(static_cast<unsigned char>(text[i])) != 'o' ||
      std::tolower(static_cast<unsigned char>(text[i + 1])) != 's')
    return false;
  char d = text[i + 2];
  if (d != ' ' && d != '\t' && d != ':') return false;
  remainder = trim_copy(text.substr(i + 3));
  return !remainder.empty();
}

std::string os_search_windows_sql(const std::string& query, int top) {
  if (top < 1) top = 1;
  if (top > 20) top = 20;
  std::string norm = normalize_query(query);
  std::vector<std::string> toks;
  std::string cur;
  for (char c : norm + " ") {
    if (c == ' ' || c == '\t') {
      auto t = sanitize_token(cur);
      if (!t.empty()) toks.push_back(t);
      cur.clear();
      if (toks.size() >= 4) break;
    } else {
      cur.push_back(c);
    }
  }
  if (toks.empty()) return {};
  std::string contains;
  for (std::size_t i = 0; i < toks.size(); ++i) {
    if (i) contains += " AND ";
    contains += "\"" + toks[i] + "*\"";
  }
  return "SELECT TOP " + std::to_string(top) +
         " System.ItemPathDisplay FROM SystemIndex WHERE CONTAINS(System.FileName,'" + contains +
         "')";
}

std::vector<std::string> os_search_argv(const std::string& backend, const std::string& query,
                                        int limit) {
  auto b = os_search_normalize_backend(backend);
  if (b.empty() || b == "auto") b = os_search_default_backend();
  if (b.empty()) return {};
  if (limit < 1) limit = 1;
  if (limit > 20) limit = 20;
  std::string q = trim_copy(query);
  if (q.empty() || q.size() > 256) return {};
  if (b == "spotlight") {
    return {"mdfind", q};
  }
  if (b == "tracker3") {
    return {"tracker3", "search", "--files", "--limit", std::to_string(limit), q};
  }
  if (b == "tracker") {
    return {"tracker3", "search", "--files", "--limit", std::to_string(limit), q};
  }
  if (b == "tracker_legacy") {
    return {"tracker", "search", "--files", "--limit", std::to_string(limit), q};
  }
  if (b == "baloo") {
    return {"baloosearch", "-l", std::to_string(limit), q};
  }
  if (b == "locate") {
    return {"locate", "-b", "-l", std::to_string(limit), "--", q};
  }
  if (b == "plocate") {
    return {"plocate", "-b", "-l", std::to_string(limit), "--", q};
  }
  if (b == "everything") {
    return {"es.exe", "-n", std::to_string(limit), q};
  }
  if (b == "everything_posix") {
    return {"es", "-n", std::to_string(limit), q};
  }
  if (b == "windows_search") {
    auto script = windows_ps_script(q, limit);
    if (script.empty()) return {};
    std::string enc = base64_encode(utf8_to_utf16le(script));
    return {"powershell",      "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass",
            "-EncodedCommand", enc};
  }
  return {};
}

std::vector<std::string> os_search_parse_output(const std::string& backend,
                                                const std::string& output,
                                                std::size_t max_results) {
  std::vector<std::string> out;
  if (output.empty() || max_results == 0) return out;
  auto b = os_search_normalize_backend(backend);
  for (auto& raw : split_lines(output)) {
    if (out.size() >= max_results) break;
    auto line = trim_copy(raw);
    if (line.empty() || line.size() > 1024) continue;
    std::string path;
    if (b == "tracker" || b == "tracker3" || b == "tracker_legacy") {
      if (line.find("file://") != std::string::npos) {
        path = strip_file_uri(line);
      } else if (looks_like_path(line)) {
        path = line;
      } else {
        continue;  // headers like "Results:" / "No results were found"
      }
    } else {
      if (!looks_like_path(line)) continue;
      path = line;
    }
    if (path.empty() || path.size() > 1024) continue;
    out.push_back(path);
  }
  return out;
}

std::vector<SearchResult> os_search_results_from_paths(const std::vector<std::string>& paths,
                                                       const std::string& backend,
                                                       std::size_t limit) {
  std::vector<SearchResult> out;
  if (limit == 0) return out;
  auto label = os_search_backend_label(backend);
  int score = 450;
  std::vector<std::string> seen_keys;
  for (auto& p : paths) {
    if (out.size() >= limit) break;
    if (p.empty() || p.size() > 1024) continue;
    auto key = to_lower_utf8(p);
    bool dup = false;
    for (auto& k : seen_keys)
      if (k == key) {
        dup = true;
        break;
      }
    if (dup) continue;
    seen_keys.push_back(key);
    SearchResult r;
    r.title = path_filename(p);
    if (r.title.empty()) r.title = p;
    auto parent = path_parent(p);
    r.subtitle = parent.empty() ? ("via " + label) : (parent + " · via " + label);
    r.path = p;
    r.payload = p;
    r.action = ResultAction::Open;
    r.score = score;
    if (score > 300) score -= 5;
    auto ext = to_lower_utf8(path_extension(p));
    if (!ext.empty()) r.kind = classify_extension(ext);
    r.kind_label = "os";
    r.category = "os";
    out.push_back(std::move(r));
  }
  return out;
}

std::vector<SearchResult> OsSearchProvider::query(const std::string& text, const Config& cfg,
                                                  std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.os_search.enabled || limit == 0) return out;
#if defined(__ANDROID__) || defined(WILFRED_IOS)
  return out;
#else
  if (!os_search_available()) return out;
  std::string remainder;
  bool explicit_os = os_search_strip_prefix(text, remainder);
  std::string q = explicit_os ? remainder : text;
  if (explicit_os) {
    if (normalize_query(q).empty()) return out;
  } else if (!os_search_should_query(q)) {
    return out;
  }
  int budget = cfg.os_search.max_results;
  if (budget < 1) budget = 1;
  if (budget > 20) budget = 20;
  if (static_cast<std::size_t>(budget) > limit) budget = static_cast<int>(limit);

  auto be = os_search_normalize_backend(cfg.os_search.backend);
  if (be.empty()) be = "auto";
  int timeout = cfg.os_search.timeout_ms;
  if (timeout < 1000) timeout = 1000;
  if (timeout > 15000) timeout = 15000;

  // Resolve one concrete backend (first available tool wins for `auto`).
  std::string use;
  std::vector<std::string> argv;
  auto try_argv = [&](const std::string& concrete, const char* tool) -> bool {
    auto a = os_search_argv(concrete, q, budget);
    if (a.empty()) return false;
    if (tool && !have_tool_cached(tool)) return false;
    argv = std::move(a);
    use = concrete;
    return true;
  };

  if (be == "auto") {
#ifdef _WIN32
    // Native SystemIndex first (official, in-process, ~10ms); Everything's
    // CLI when installed; PowerShell ADODB last.
    {
      std::vector<std::string> com_paths;
      if (os_windows_search_com(q, budget, com_paths))
        return os_search_results_from_paths(com_paths, "windows_search",
                                            static_cast<std::size_t>(budget));
    }
    if (have_tool_cached("es") && try_argv("everything_posix", "es")) {
      use = "everything";
    } else {
      if (!try_argv("windows_search", "powershell")) return out;
      use = "windows_search";
    }
#elif defined(__APPLE__)
    if (!try_argv("spotlight", "mdfind")) return out;
#else
    if (try_argv("tracker3", "tracker3")) {
      use = "tracker";
    } else if (try_argv("tracker_legacy", "tracker")) {
      use = "tracker";
    } else if (try_argv("baloo", "baloosearch")) {
      // use stays "baloo"
    } else if (try_argv("locate", "locate")) {
      // use stays "locate"
    } else if (try_argv("plocate", "plocate")) {
      use = "locate";
    } else {
      return out;
    }
#endif
  } else if (be == "tracker") {
    if (try_argv("tracker3", "tracker3")) {
      use = "tracker";
    } else if (!try_argv("tracker_legacy", "tracker")) {
      return out;
    }
  } else if (be == "locate") {
    if (try_argv("locate", "locate")) {
    } else if (!try_argv("plocate", "plocate")) {
      return out;
    }
    use = "locate";
  } else if (be == "everything") {
#ifdef _WIN32
    if (!try_argv("everything_posix", "es")) return out;
    use = "everything";
#else
    if (!try_argv("everything_posix", "es")) return out;
    use = "everything";
#endif
  } else if (be == "windows_search") {
#ifdef _WIN32
    // Native SystemIndex first; PowerShell ADODB when COM is unavailable
    // (service off). Skips Everything: explicit means official only.
    {
      std::vector<std::string> com_paths;
      if (os_windows_search_com(q, budget, com_paths))
        return os_search_results_from_paths(com_paths, "windows_search",
                                            static_cast<std::size_t>(budget));
    }
    if (!try_argv("windows_search", "powershell")) return out;
    use = "windows_search";
#else
    return out;  // SystemIndex exists on Windows only
#endif
  } else if (be == "spotlight") {
#if defined(__APPLE__) && !defined(WILFRED_IOS)
    if (!try_argv("spotlight", "mdfind")) return out;
#else
    return out;  // mdfind exists on macOS only
#endif
  } else if (be == "baloo") {
    if (!try_argv("baloo", "baloosearch")) return out;
  } else {
    return out;
  }

  std::string body;
  int rc = os_run_capture(argv, timeout, 128 * 1024, body);
  if (rc != 0 || body.empty()) return out;
  auto paths = os_search_parse_output(use, body, static_cast<std::size_t>(budget));
  if (paths.empty()) return out;
  return os_search_results_from_paths(paths, use, static_cast<std::size_t>(budget));
#endif
}

}  // namespace wilfred
