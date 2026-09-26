#include "wilfred/ipc/http.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace wilfred {
#ifdef _WIN32
using Socket = SOCKET;
static constexpr Socket kInvalidSock = INVALID_SOCKET;
static void close_sock(Socket s) { closesocket(s); }
static int sock_send(Socket s, const char* p, int n) { return send(s, p, n, 0); }
static int sock_recv(Socket s, char* p, int n) { return recv(s, p, n, 0); }
#else
using Socket = int;
static constexpr Socket kInvalidSock = -1;
static void close_sock(Socket s) { close(s); }
static int sock_send(Socket s, const char* p, int n) { return static_cast<int>(send(s, p, n, 0)); }
static int sock_recv(Socket s, char* p, int n) { return static_cast<int>(recv(s, p, n, 0)); }
#endif

static std::once_flag g_sock_once;
static void ensure_sockets() {
#ifdef _WIN32
  std::call_once(g_sock_once, [] {
    WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
  });
#else
  (void)g_sock_once;
#endif
}

static std::string url_decode(std::string_view s) {
  std::string o;
  for (std::size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size()) {
      auto hex = [](char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
      };
      o.push_back(static_cast<char>((hex(s[i + 1]) << 4) | hex(s[i + 2])));
      i += 2;
    } else if (s[i] == '+')
      o.push_back(' ');
    else
      o.push_back(s[i]);
  }
  return o;
}

static bool parse_http_request(const std::string& raw, HttpApiRequest& req, std::size_t& body_need) {
  auto hdr_end = raw.find("\r\n\r\n");
  if (hdr_end == std::string::npos) {
    body_need = 0;
    return false;
  }
  auto line_end = raw.find("\r\n");
  if (line_end == std::string::npos) return false;
  std::istringstream first(raw.substr(0, line_end));
  first >> req.method;
  std::string target;
  first >> target;
  auto qpos = target.find('?');
  if (qpos == std::string::npos) {
    req.path = target;
  } else {
    req.path = target.substr(0, qpos);
    req.query = url_decode(target.substr(qpos + 1));
  }
  std::size_t content_len = 0;
  std::size_t i = line_end + 2;
  while (i < hdr_end) {
    auto nl = raw.find("\r\n", i);
    if (nl == std::string::npos || nl > hdr_end) break;
    auto line = raw.substr(i, nl - i);
    auto colon = line.find(':');
    if (colon != std::string::npos) {
      auto k = to_lower_utf8(line.substr(0, colon));
      auto v = line.substr(colon + 1);
      while (!v.empty() && (v.front() == ' ' || v.front() == '\t')) v.erase(v.begin());
      if (k == "content-length") {
        try {
          content_len = static_cast<std::size_t>(std::stoul(v));
        } catch (...) {
        }
      } else if (k == "x-wilfred-token" || k == "authorization") {
        if (k == "authorization" && v.rfind("Bearer ", 0) == 0) v = v.substr(7);
        req.token = v;
      }
    }
    i = nl + 2;
  }
  auto body_start = hdr_end + 4;
  if (raw.size() < body_start + content_len) {
    body_need = body_start + content_len - raw.size();
    return false;
  }
  req.body = raw.substr(body_start, content_len);
  body_need = 0;
  return true;
}

static std::string encode_http(const HttpApiResponse& r) {
  std::ostringstream os;
  os << "HTTP/1.1 " << r.status << " " << (r.status == 200 ? "OK" : r.status == 401 ? "Unauthorized" : "Error")
     << "\r\n";
  os << "Content-Type: " << (r.content_type.empty() ? "application/json" : r.content_type) << "\r\n";
  os << "Content-Length: " << r.body.size() << "\r\n";
  os << "Connection: close\r\n";
  os << "Access-Control-Allow-Origin: *\r\n\r\n";
  os << r.body;
  return os.str();
}

struct HttpApiServer::Impl {
  std::string bind;
  int port{0};
  std::string token;
  HttpApiHandler handler;
  std::atomic<bool> running{false};
  Socket listen_fd{kInvalidSock};
  std::thread th;

  void loop() {
    while (running) {
      sockaddr_in addr{};
#ifdef _WIN32
      int len = sizeof(addr);
#else
      socklen_t len = sizeof(addr);
#endif
      Socket c = accept(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
      if (c == kInvalidSock) {
        if (!running) break;
        continue;
      }
      char ip[64]{};
      inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
      std::string raw;
      char buf[4096];
      for (;;) {
        int n = sock_recv(c, buf, sizeof(buf));
        if (n <= 0) break;
        raw.append(buf, static_cast<std::size_t>(n));
        HttpApiRequest req;
        req.peer = ip;
        std::size_t need = 0;
        if (parse_http_request(raw, req, need)) {
          HttpApiResponse resp;
          bool local = req.peer == "127.0.0.1" || req.peer == "::1" || req.peer == "0.0.0.0";
          if (req.method != "OPTIONS" && !token.empty() && req.token != token) {
            resp.status = 401;
            resp.body = "{\"ok\":false,\"error\":\"unauthorized\"}";
          } else if (req.method != "OPTIONS" && token.empty() && !local) {
            resp.status = 401;
            resp.body = "{\"ok\":false,\"error\":\"token required for remote clients\"}";
          } else {
            try {
              resp = handler(req);
            } catch (...) {
              resp.status = 500;
              resp.body = "{\"ok\":false,\"error\":\"handler failed\"}";
            }
          }
          auto out = encode_http(resp);
          sock_send(c, out.data(), static_cast<int>(out.size()));
          break;
        }
        if (raw.size() > 4 * 1024 * 1024) break;
      }
      close_sock(c);
    }
  }
};

HttpApiServer::HttpApiServer() : impl_(std::make_unique<Impl>()) {}
HttpApiServer::~HttpApiServer() { stop(); }

int HttpApiServer::port() const { return impl_ ? impl_->port : 0; }

bool HttpApiServer::start(const std::string& bind, int port, const std::string& token, HttpApiHandler handler) {
  ensure_sockets();
  stop();
  impl_->bind = bind.empty() ? "127.0.0.1" : bind;
  impl_->port = port;
  impl_->token = token;
  impl_->handler = std::move(handler);
  impl_->listen_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (impl_->listen_fd == kInvalidSock) return false;
  int yes = 1;
  setsockopt(impl_->listen_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&yes), sizeof(yes));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<std::uint16_t>(port));
  if (inet_pton(AF_INET, impl_->bind.c_str(), &addr.sin_addr) != 1) {
    close_sock(impl_->listen_fd);
    impl_->listen_fd = kInvalidSock;
    return false;
  }
  if (::bind(impl_->listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close_sock(impl_->listen_fd);
    impl_->listen_fd = kInvalidSock;
    return false;
  }
  if (listen(impl_->listen_fd, 16) != 0) {
    close_sock(impl_->listen_fd);
    impl_->listen_fd = kInvalidSock;
    return false;
  }
  impl_->running = true;
  impl_->th = std::thread([this] { impl_->loop(); });
  log_info("api", "HTTP API listening on " + impl_->bind + ":" + std::to_string(port));
  return true;
}

void HttpApiServer::stop() {
  if (!impl_) return;
  impl_->running = false;
  if (impl_->listen_fd != kInvalidSock) {
    close_sock(impl_->listen_fd);
    impl_->listen_fd = kInvalidSock;
  }
  if (impl_->th.joinable()) impl_->th.join();
}

#ifdef _WIN32
static std::wstring utf16(const std::string& s) { return utf8_to_wide(s); }

static bool parse_url(const std::string& url, bool& https, std::wstring& host, INTERNET_PORT& port,
                      std::wstring& path) {
  https = url.rfind("https://", 0) == 0;
  auto rest = url;
  if (url.rfind("https://", 0) == 0)
    rest = url.substr(8);
  else if (url.rfind("http://", 0) == 0)
    rest = url.substr(7);
  else
    return false;
  auto slash = rest.find('/');
  auto authority = slash == std::string::npos ? rest : rest.substr(0, slash);
  path = utf16(slash == std::string::npos ? "/" : rest.substr(slash));
  auto colon = authority.find(':');
  if (colon == std::string::npos) {
    host = utf16(authority);
    port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
  } else {
    host = utf16(authority.substr(0, colon));
    try {
      port = static_cast<INTERNET_PORT>(std::stoi(authority.substr(colon + 1)));
    } catch (...) {
      return false;
    }
  }
  return true;
}
#endif

#ifndef _WIN32
static bool parse_url_posix(const std::string& url, bool& https, std::string& host, int& port, std::string& path) {
  https = url.rfind("https://", 0) == 0;
  auto rest = url;
  if (url.rfind("https://", 0) == 0)
    rest = url.substr(8);
  else if (url.rfind("http://", 0) == 0)
    rest = url.substr(7);
  else
    return false;
  auto slash = rest.find('/');
  auto authority = slash == std::string::npos ? rest : rest.substr(0, slash);
  path = slash == std::string::npos ? "/" : rest.substr(slash);
  auto colon = authority.find(':');
  if (colon == std::string::npos) {
    host = authority;
    port = https ? 443 : 80;
  } else {
    host = authority.substr(0, colon);
    try {
      port = std::stoi(authority.substr(colon + 1));
    } catch (...) {
      return false;
    }
  }
  return true;
}
#endif

std::string http_get(const std::string& url, const std::string& token, int timeout_ms) {
#ifdef _WIN32
  bool https = false;
  std::wstring host, path;
  INTERNET_PORT port = 80;
  if (!parse_url(url, https, host, port, path)) return {};
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return {};
  WinHttpSetTimeouts(ses, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
  HINTERNET con = WinHttpConnect(ses, host.c_str(), port, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return {};
  }
  DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(con, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return {};
  }
  std::wstring hdr;
  if (!token.empty()) hdr = L"X-Wilfred-Token: " + utf16(token) + L"\r\n";
  BOOL ok = WinHttpSendRequest(req, hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : hdr.c_str(),
                               hdr.empty() ? 0 : static_cast<DWORD>(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (ok) ok = WinHttpReceiveResponse(req, nullptr);
  std::string body;
  if (ok) {
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(req, &avail) && avail) {
      std::string chunk(avail, '\0');
      DWORD got = 0;
      WinHttpReadData(req, chunk.data(), avail, &got);
      body.append(chunk.data(), got);
    }
  }
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return body;
#else
  bool https = false;
  std::string host, path;
  int port = 80;
  if (!parse_url_posix(url, https, host, port, path)) return {};
  if (https) return {};
  ensure_sockets();
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_INET;
  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) return {};
  Socket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (s == kInvalidSock) {
    freeaddrinfo(res);
    return {};
  }
  if (connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
    close_sock(s);
    freeaddrinfo(res);
    return {};
  }
  freeaddrinfo(res);
  std::ostringstream req;
  req << "GET " << path << " HTTP/1.1\r\nHost: " << host << "\r\nConnection: close\r\n";
  if (!token.empty()) req << "X-Wilfred-Token: " << token << "\r\n";
  req << "\r\n";
  auto rq = req.str();
  sock_send(s, rq.data(), static_cast<int>(rq.size()));
  std::string raw;
  char buf[4096];
  for (;;) {
    int n = sock_recv(s, buf, sizeof(buf));
    if (n <= 0) break;
    raw.append(buf, static_cast<std::size_t>(n));
  }
  close_sock(s);
  auto pos = raw.find("\r\n\r\n");
  return pos == std::string::npos ? raw : raw.substr(pos + 4);
  (void)timeout_ms;
#endif
}

bool http_put(const std::string& url, const std::string& token, const std::string& body, int timeout_ms,
              std::string* err) {
#ifdef _WIN32
  bool https = false;
  std::wstring host, path;
  INTERNET_PORT port = 80;
  if (!parse_url(url, https, host, port, path)) {
    if (err) *err = "invalid url";
    return false;
  }
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                              WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return false;
  WinHttpSetTimeouts(ses, timeout_ms, timeout_ms, timeout_ms, timeout_ms);
  HINTERNET con = WinHttpConnect(ses, host.c_str(), port, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return false;
  }
  DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(con, L"PUT", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return false;
  }
  std::wstring hdr = L"Content-Type: application/octet-stream\r\n";
  if (!token.empty()) hdr += L"X-Wilfred-Token: " + utf16(token) + L"\r\n";
  BOOL ok = WinHttpSendRequest(req, hdr.c_str(), static_cast<DWORD>(-1), (LPVOID)body.data(),
                               static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
  if (ok) ok = WinHttpReceiveResponse(req, nullptr);
  if (!ok && err) *err = "http put failed";
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return ok == TRUE;
#else
  bool https = false;
  std::string host, path;
  int port = 80;
  if (!parse_url_posix(url, https, host, port, path) || https) {
    if (err) *err = https ? "https sync requires Windows" : "invalid url";
    return false;
  }
  ensure_sockets();
  addrinfo hints{};
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_family = AF_INET;
  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0) return false;
  Socket s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (s == kInvalidSock) {
    freeaddrinfo(res);
    return false;
  }
  if (connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen)) != 0) {
    close_sock(s);
    freeaddrinfo(res);
    return false;
  }
  freeaddrinfo(res);
  std::ostringstream req;
  req << "PUT " << path << " HTTP/1.1\r\nHost: " << host << "\r\nConnection: close\r\n";
  req << "Content-Type: application/octet-stream\r\nContent-Length: " << body.size() << "\r\n";
  if (!token.empty()) req << "X-Wilfred-Token: " << token << "\r\n";
  req << "\r\n";
  auto head = req.str();
  sock_send(s, head.data(), static_cast<int>(head.size()));
  sock_send(s, body.data(), static_cast<int>(body.size()));
  char buf[256];
  sock_recv(s, buf, sizeof(buf));
  close_sock(s);
  (void)timeout_ms;
  return true;
#endif
}

}  // namespace wilfred
