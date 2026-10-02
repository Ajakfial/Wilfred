#include "wilfred/search/nettools.hpp"

#include "wilfred/core/utf8.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

namespace wilfred {

std::vector<std::string> dns_lookup(const std::string& host) {
  std::vector<std::string> out;
  if (host.empty() || host.size() > 253) return out;
  for (char c : host) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_' ||
          c == ':'))
      return out;
  }
#ifdef _WIN32
  WSADATA wsa{};
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return out;
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  auto w = utf8_to_wide(host);
  // getaddrinfoA for narrow host.
  if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0) {
    for (auto* p = res; p && out.size() < 8; p = p->ai_next) {
      char buf[128]{};
      if (p->ai_family == AF_INET) {
        auto* a = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        inet_ntop(AF_INET, &a->sin_addr, buf, sizeof(buf));
      } else if (p->ai_family == AF_INET6) {
        auto* a = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
        inet_ntop(AF_INET6, &a->sin6_addr, buf, sizeof(buf));
      } else {
        continue;
      }
      std::string ip(buf);
      if (!ip.empty() && std::find(out.begin(), out.end(), ip) == out.end()) out.push_back(ip);
    }
    freeaddrinfo(res);
  }
  WSACleanup();
#else
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* res = nullptr;
  if (getaddrinfo(host.c_str(), nullptr, &hints, &res) == 0) {
    for (auto* p = res; p && out.size() < 8; p = p->ai_next) {
      char buf[128]{};
      if (p->ai_family == AF_INET) {
        auto* a = reinterpret_cast<sockaddr_in*>(p->ai_addr);
        inet_ntop(AF_INET, &a->sin_addr, buf, sizeof(buf));
      } else if (p->ai_family == AF_INET6) {
        auto* a = reinterpret_cast<sockaddr_in6*>(p->ai_addr);
        inet_ntop(AF_INET6, &a->sin6_addr, buf, sizeof(buf));
      } else {
        continue;
      }
      std::string ip(buf);
      if (!ip.empty() && std::find(out.begin(), out.end(), ip) == out.end()) out.push_back(ip);
    }
    freeaddrinfo(res);
  }
#endif
  return out;
}

std::string ping_summary(const std::string& host) {
  if (host.empty() || host.size() > 253) return {};
  for (char c : host) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_' ||
          c == ':'))
      return {};
  }
#ifdef _WIN32
  std::string cmd = "ping -n 1 -w 1500 " + host + " 2>&1";
  FILE* f = _popen(cmd.c_str(), "r");
  if (!f) return {};
#else
  std::string cmd = "ping -c 1 -W 2 " + host + " 2>&1";
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) return {};
#endif
  std::string out;
  char buf[512];
  while (fgets(buf, sizeof(buf), f)) {
    out += buf;
    if (out.size() > 2048) break;
  }
#ifdef _WIN32
  _pclose(f);
#else
  pclose(f);
#endif
  // Extract a concise line: look for time= / time< / ms.
  std::string low = to_lower_utf8(out);
  auto pos = low.find("time");
  if (pos != std::string::npos) {
    auto eol = out.find('\n', pos);
    std::string line = out.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    return line.size() > 120 ? line.substr(0, 120) : line;
  }
  if (low.find("unreachable") != std::string::npos) return "host unreachable";
  if (low.find("could not find") != std::string::npos ||
      low.find("not known") != std::string::npos || low.find("failure") != std::string::npos)
    return "lookup failed";
  // Fallback: first non-empty line.
  std::string first;
  for (char c : out) {
    if (c == '\n') break;
    first.push_back(c);
  }
  if (first.size() > 120) first.resize(120);
  return first.empty() ? std::string("no response") : first;
}

std::string public_ip() {
#ifdef _WIN32
  // Keep dependency-free: use WinHTTP directly would duplicate minis helper;
  // shell out to curl when available, else empty.
  FILE* f = _popen("curl -fsS --max-time 3 https://api.ipify.org 2>NUL", "r");
  if (!f) return {};
  char buf[64]{};
  std::string o;
  if (fgets(buf, sizeof(buf), f)) o = buf;
  _pclose(f);
#else
  FILE* f = popen("curl -fsS --max-time 3 https://api.ipify.org 2>/dev/null", "r");
  if (!f) return {};
  char buf[64]{};
  std::string o;
  if (fgets(buf, sizeof(buf), f)) o = buf;
  pclose(f);
#endif
  while (!o.empty() && (o.back() == '\n' || o.back() == '\r' || o.back() == ' ')) o.pop_back();
  if (o.size() > 64) return {};
  // Basic sanity: digits/dots/colons only.
  for (char c : o) {
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ':')) return {};
  }
  return o;
}

bool looks_like_email(const std::string& s) {
  auto t = s;
  while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
  while (!t.empty() && (t.back() == ' ' || t.back() == '\t' || t.back() == '\n' || t.back() == '\r'))
    t.pop_back();
  if (t.size() > 254 || t.find(' ') != std::string::npos) return false;
  auto at = t.find('@');
  if (at == std::string::npos || at == 0 || at + 1 >= t.size()) return false;
  if (t.find('@', at + 1) != std::string::npos) return false;
  auto dot = t.find('.', at);
  return dot != std::string::npos && dot + 1 < t.size();
}

bool looks_like_url_text(const std::string& s) {
  auto t = s;
  while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
  auto l = to_lower_utf8(t);
  if (l.rfind("http://", 0) == 0 || l.rfind("https://", 0) == 0 || l.rfind("www.", 0) == 0)
    return true;
  // Emails contain '@' — never a bare URL.
  if (t.find('@') != std::string::npos) return false;
  // bare domain with TLD + path?
  auto slash = t.find('/');
  std::string host = slash == std::string::npos ? t : t.substr(0, slash);
  if (host.size() > 253 || host.find(' ') != std::string::npos) return false;
  if (host.find('@') != std::string::npos) return false;
  auto dot = host.find_last_of('.');
  if (dot == std::string::npos || dot + 1 >= host.size()) return false;
  std::string tld = to_lower_utf8(host.substr(dot + 1));
  // Trim trailing punctuation.
  while (!tld.empty() && (tld.back() == '.' || tld.back() == ',' || tld.back() == ')')) tld.pop_back();
  static const char* known[] = {"com", "org", "net", "io", "dev", "app", "edu", "gov", nullptr};
  for (auto** p = known; *p; ++p)
    if (tld == *p) return true;
  return false;
}

bool looks_like_path_text(const std::string& s) {
  auto t = s;
  while (!t.empty() && (t.front() == ' ' || t.front() == '\t' || t.front() == '"' || t.front() == '\'')) t.erase(t.begin());
  if (t.size() < 2) return false;
  if (t[0] == '/' || t[0] == '~') return true;
  if (t.size() >= 3 && std::isalpha(static_cast<unsigned char>(t[0])) && t[1] == ':' &&
      (t[2] == '\\' || t[2] == '/'))
    return true;
  if (t.rfind("\\\\", 0) == 0) return true;
  if (t.find('/') != std::string::npos && t.find('.') != std::string::npos) {
    // Avoid flagging sentences: require no spaces.
    return t.find(' ') == std::string::npos && t.find('\n') == std::string::npos;
  }
  return false;
}

bool looks_like_ip_text(const std::string& s) {
  auto t = s;
  while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
  while (!t.empty() && (t.back() == ' ' || t.back() == '\t')) t.pop_back();
  if (t.empty() || t.size() > 64) return false;
  int dots = 0, colons = 0;
  for (char c : t) {
    if (c == '.') ++dots;
    if (c == ':') ++colons;
    if (!(std::isdigit(static_cast<unsigned char>(c)) || c == '.' || c == ':' ||
          (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
      return false;
  }
  return dots == 3 || colons >= 2;
}

bool looks_like_code_text(const std::string& s) {
  if (s.size() < 8) return false;
  int markers = 0;
  if (s.find('{') != std::string::npos && s.find('}') != std::string::npos) ++markers;
  if (s.find(';') != std::string::npos) ++markers;
  if (s.find("function") != std::string::npos || s.find("def ") != std::string::npos ||
      s.find("class ") != std::string::npos || s.find("#include") != std::string::npos ||
      s.find("import ") != std::string::npos || s.find("=>") != std::string::npos)
    markers += 2;
  if (s.find('\n') != std::string::npos) ++markers;
  if (s.find("()") != std::string::npos || s.find("==") != std::string::npos) ++markers;
  return markers >= 2;
}

std::string clip_type_of(const std::string& s) {
  if (looks_like_url_text(s)) return "url";
  if (looks_like_email(s)) return "email";
  if (looks_like_path_text(s)) return "path";
  if (looks_like_ip_text(s)) return "ip";
  if (looks_like_code_text(s)) return "code";
  return "text";
}

}  // namespace wilfred
