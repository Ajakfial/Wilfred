#include "wilfred/search/mpris_dbus.hpp"

#include <cstdint>
#include <string>
#include <vector>

#ifndef _WIN32
// struct timeval for the socket timeouts below: transient on glibc,
// absent on NetBSD without the direct include.
#include <sys/time.h>
#endif

namespace wilfred {
namespace {

struct Buf {
  std::vector<unsigned char> b;
  void align(std::size_t a) {
    while (b.size() % a)
      b.push_back(0);
  }
  void u8(std::uint8_t v) { b.push_back(v); }
  void u32(std::uint32_t v) {
    align(4);
    b.push_back(static_cast<unsigned char>(v & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 8) & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 16) & 0xff));
    b.push_back(static_cast<unsigned char>((v >> 24) & 0xff));
  }
  void sig(const std::string& s) {
    u8(static_cast<std::uint8_t>(s.size()));
    for (char c : s)
      b.push_back(static_cast<unsigned char>(c));
    b.push_back(0);
  }
  void str(const std::string& s) {
    align(4);
    u32(static_cast<std::uint32_t>(s.size()));
    for (char c : s)
      b.push_back(static_cast<unsigned char>(c));
    b.push_back(0);
  }
  void field(std::uint8_t code, const std::string& sig_type, const std::string& value) {
    align(8);
    u8(code);
    sig(sig_type);
    str(value);
  }
};

std::uint32_t rd_u32le(const unsigned char* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::string mpris_method_for(const std::string& id) {
  if (id == "play") return "Play";
  if (id == "pause") return "Pause";
  if (id == "playpause") return "PlayPause";
  if (id == "next") return "Next";
  if (id == "prev") return "Previous";
  if (id == "stop") return "Stop";
  return {};
}

std::vector<unsigned char> mpris_build_method_call(std::uint32_t serial,
                                                   const std::string& destination,
                                                   const std::string& path,
                                                   const std::string& interface,
                                                   const std::string& member) {
  Buf fields;
  if (!destination.empty()) fields.field(6, "s", destination);
  fields.field(1, "o", path);
  if (!interface.empty()) fields.field(2, "s", interface);
  fields.field(3, "s", member);

  Buf msg;
  msg.b.push_back('l');
  msg.b.push_back(1);
  msg.b.push_back(0);
  msg.b.push_back(1);
  msg.u32(0);
  msg.u32(serial);
  msg.u32(static_cast<std::uint32_t>(fields.b.size()));
  for (auto c : fields.b)
    msg.b.push_back(c);
  while (msg.b.size() % 8)
    msg.b.push_back(0);
  return msg.b;
}

bool mpris_parse_names_reply(const unsigned char* data, std::size_t size,
                             std::vector<std::string>& out_names) {
  out_names.clear();
  if (size < 16) return false;
  if (data[0] != 'l' && data[0] != 'B') return false;
  if (data[0] != 'l') return false;
  std::uint8_t type = data[1];
  if (type == 3) return false;
  if (type != 2) return false;
  std::uint32_t body_len = rd_u32le(data + 4);
  std::uint32_t hlen = rd_u32le(data + 12);
  std::size_t hdr_end = 16 + hlen;
  if (hdr_end > size) return false;
  std::size_t body_off = hdr_end;
  while (body_off % 8)
    ++body_off;
  if (body_off + body_len > size) return false;
  if (body_len < 4) return false;
  std::size_t p = body_off;
  auto need = [&](std::size_t n) { return p + n <= body_off + body_len; };
  if (!need(4)) return false;
  std::uint32_t arr_len = rd_u32le(data + p);
  p += 4;
  std::size_t arr_end = p + arr_len;
  if (arr_end > body_off + body_len) return false;
  while (p < arr_end) {
    while (p % 4)
      ++p;
    if (!need(4) || p + 4 > arr_end) return false;
    std::uint32_t slen = rd_u32le(data + p);
    p += 4;
    if (slen > 4096 || p + slen + 1 > arr_end) return false;
    out_names.emplace_back(reinterpret_cast<const char*>(data + p), slen);
    p += slen + 1;
  }
  return true;
}

}  // namespace wilfred

#if !defined(_WIN32) && !defined(__APPLE__)
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wilfred {
namespace {

bool send_all(int fd, const unsigned char* d, std::size_t n) {
  std::size_t off = 0;
  while (off < n) {
    ssize_t w = ::send(fd, d + off, n - off, MSG_NOSIGNAL);
    if (w <= 0) {
      if (errno == EINTR) continue;
      return false;
    }
    off += static_cast<std::size_t>(w);
  }
  return true;
}

bool recv_exact(int fd, unsigned char* d, std::size_t n) {
  std::size_t off = 0;
  while (off < n) {
    ssize_t r = ::recv(fd, d + off, n - off, 0);
    if (r <= 0) {
      if (r < 0 && errno == EINTR) continue;
      return false;
    }
    off += static_cast<std::size_t>(r);
  }
  return true;
}

bool recv_line(int fd, std::string& out) {
  out.clear();
  char c = 0;
  while (out.size() < 4096) {
    ssize_t r = ::recv(fd, &c, 1, 0);
    if (r <= 0) {
      if (r < 0 && errno == EINTR) continue;
      return false;
    }
    out.push_back(c);
    if (out.size() >= 2 && out[out.size() - 2] == '\r' && out[out.size() - 1] == '\n') return true;
  }
  return false;
}

struct Bus {
  int fd{-1};
  std::uint32_t serial{1};
  ~Bus() {
    if (fd >= 0) ::close(fd);
  }
  bool connect_session(std::string& error) {
    std::string addr;
    if (const char* e = std::getenv("DBUS_SESSION_BUS_ADDRESS")) addr = e;
    std::string path;
    bool abstract = false;
    if (!addr.empty()) {
      std::string first = addr;
      if (auto sc = first.find(';'); sc != std::string::npos) first.resize(sc);
      auto pp = first.find("path=");
      auto ap = first.find("abstract=");
      if (pp != std::string::npos) {
        auto end = first.find(',', pp);
        path = first.substr(pp + 5, end == std::string::npos ? std::string::npos : end - pp - 5);
      } else if (ap != std::string::npos) {
        auto end = first.find(',', ap);
        path = first.substr(ap + 9, end == std::string::npos ? std::string::npos : end - ap - 9);
        abstract = true;
      }
    }
    if (path.empty()) {
      long uid = static_cast<long>(::getuid());
      path = "/run/user/" + std::to_string(uid) + "/bus";
    }
    fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
      error = "no session bus (socket failed)";
      return false;
    }
    struct timeval tv {};
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_un sa{};
    sa.sun_family = AF_UNIX;
    if (abstract) {
      if (path.size() + 1 >= sizeof(sa.sun_path)) {
        error = "no session bus (bad address)";
        return false;
      }
      sa.sun_path[0] = '\0';
      std::memcpy(sa.sun_path + 1, path.c_str(), path.size());
      socklen_t len = static_cast<socklen_t>(sizeof(sa.sun_family) + 1 + path.size());
      if (::connect(fd, reinterpret_cast<sockaddr*>(&sa), len) != 0) {
        error = "no session bus";
        return false;
      }
    } else {
      if (path.size() >= sizeof(sa.sun_path)) {
        error = "no session bus (bad address)";
        return false;
      }
      std::memcpy(sa.sun_path, path.c_str(), path.size() + 1);
      if (::connect(fd, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) != 0) {
        error = "no session bus";
        return false;
      }
    }
    long uid = static_cast<long>(::getuid());
    char hex[32]{};
    std::snprintf(hex, sizeof(hex), "%lx", static_cast<unsigned long>(uid));
    std::string auth = std::string("AUTH EXTERNAL ") + hex + "\r\n";
    if (!send_all(fd, reinterpret_cast<const unsigned char*>(auth.data()), auth.size())) {
      error = "bus auth send failed";
      return false;
    }
    std::string line;
    if (!recv_line(fd, line) || line.rfind("OK", 0) != 0) {
      error = "bus auth rejected";
      return false;
    }
    const char* neg = "NEGOTIATE_UNIX_FD\r\n";
    send_all(fd, reinterpret_cast<const unsigned char*>(neg), std::strlen(neg));
    recv_line(fd, line);
    const char* begin = "BEGIN\r\n";
    if (!send_all(fd, reinterpret_cast<const unsigned char*>(begin), std::strlen(begin))) {
      error = "bus begin failed";
      return false;
    }
    auto hello = mpris_build_method_call(serial++, "org.freedesktop.DBus", "/org/freedesktop/DBus",
                                         "org.freedesktop.DBus", "Hello");
    if (!send_all(fd, hello.data(), hello.size())) {
      error = "bus hello send failed";
      return false;
    }
    unsigned char hdr[16]{};
    if (!recv_exact(fd, hdr, 16)) {
      error = "bus hello reply failed";
      return false;
    }
    std::uint32_t body_len =
        (static_cast<std::uint32_t>(hdr[4])) | (static_cast<std::uint32_t>(hdr[5]) << 8) |
        (static_cast<std::uint32_t>(hdr[6]) << 16) | (static_cast<std::uint32_t>(hdr[7]) << 24);
    std::uint32_t hlen =
        (static_cast<std::uint32_t>(hdr[12])) | (static_cast<std::uint32_t>(hdr[13]) << 8) |
        (static_cast<std::uint32_t>(hdr[14]) << 16) | (static_cast<std::uint32_t>(hdr[15]) << 24);
    std::size_t rest = hlen;
    while ((16 + rest) % 8)
      ++rest;
    rest += body_len;
    if (rest > 1 << 20) {
      error = "bus hello reply too large";
      return false;
    }
    std::vector<unsigned char> tail(rest);
    if (rest && !recv_exact(fd, tail.data(), rest)) {
      error = "bus hello reply failed";
      return false;
    }
    if (hdr[1] == 3) {
      error = "bus hello rejected";
      return false;
    }
    return true;
  }

  bool call_no_args(const std::string& dest, const std::string& path, const std::string& iface,
                    const std::string& member, std::vector<unsigned char>& reply,
                    std::string& error) {
    auto msg = mpris_build_method_call(serial++, dest, path, iface, member);
    if (!send_all(fd, msg.data(), msg.size())) {
      error = "send failed";
      return false;
    }
    unsigned char hdr[16]{};
    if (!recv_exact(fd, hdr, 16)) {
      error = "reply recv failed";
      return false;
    }
    if (hdr[0] != 'l') {
      error = "bad endian";
      return false;
    }
    std::uint8_t type = hdr[1];
    std::uint32_t body_len =
        (static_cast<std::uint32_t>(hdr[4])) | (static_cast<std::uint32_t>(hdr[5]) << 8) |
        (static_cast<std::uint32_t>(hdr[6]) << 16) | (static_cast<std::uint32_t>(hdr[7]) << 24);
    std::uint32_t hlen =
        (static_cast<std::uint32_t>(hdr[12])) | (static_cast<std::uint32_t>(hdr[13]) << 8) |
        (static_cast<std::uint32_t>(hdr[14]) << 16) | (static_cast<std::uint32_t>(hdr[15]) << 24);
    if (hlen > 1 << 16 || body_len > 1 << 20) {
      error = "reply too large";
      return false;
    }
    std::size_t rest = hlen;
    while ((16 + rest) % 8)
      ++rest;
    rest += body_len;
    reply.assign(16 + rest, 0);
    std::memcpy(reply.data(), hdr, 16);
    if (rest && !recv_exact(fd, reply.data() + 16, rest)) {
      error = "reply body recv failed";
      return false;
    }
    if (type == 3) {
      error = "method error";
      return false;
    }
    if (type != 2) {
      error = "unexpected reply";
      return false;
    }
    return true;
  }
};

}  // namespace

std::vector<std::string> mpris_list_players(std::string& error) {
  std::vector<std::string> out;
  Bus bus;
  if (!bus.connect_session(error)) return out;
  std::vector<unsigned char> reply;
  if (!bus.call_no_args("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                        "ListNames", reply, error))
    return {};
  std::vector<std::string> names;
  if (!mpris_parse_names_reply(reply.data(), reply.size(), names)) {
    error = "could not parse bus names";
    return {};
  }
  for (auto& n : names)
    if (n.rfind("org.mpris.MediaPlayer2.", 0) == 0) out.push_back(n);
  if (out.empty()) error = "no MPRIS players found";
  return out;
}

bool mpris_media_action(const std::string& id, std::string& error) {
  auto method = mpris_method_for(id);
  if (method.empty()) {
    error = "unknown media action '" + id + "'";
    return false;
  }
  Bus bus;
  if (!bus.connect_session(error)) return false;
  std::vector<unsigned char> reply;
  std::string list_err;
  if (!bus.call_no_args("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                        "ListNames", reply, list_err)) {
    error = list_err.empty() ? "could not list players" : list_err;
    return false;
  }
  std::vector<std::string> names;
  if (!mpris_parse_names_reply(reply.data(), reply.size(), names)) {
    error = "could not parse bus names";
    return false;
  }
  std::vector<std::string> players;
  for (auto& n : names)
    if (n.rfind("org.mpris.MediaPlayer2.", 0) == 0) players.push_back(n);
  if (players.empty()) {
    error = "no MPRIS players found";
    return false;
  }
  bool any = false;
  std::string last_err;
  for (auto& p : players) {
    std::vector<unsigned char> r2;
    if (bus.call_no_args(p, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", method, r2,
                         last_err))
      any = true;
  }
  if (!any) error = last_err.empty() ? "all players failed" : last_err;
  return any;
}

}  // namespace wilfred

#else

// Portable stubs so tests link everywhere; real bus only on Linux.
namespace wilfred {

std::vector<std::string> mpris_list_players(std::string& error) {
  error = "MPRIS D-Bus is Linux-only";
  return {};
}

bool mpris_media_action(const std::string& id, std::string& error) {
  if (mpris_method_for(id).empty()) {
    error = "unknown media action '" + id + "'";
    return false;
  }
  error = "MPRIS D-Bus is Linux-only";
  return false;
}

}  // namespace wilfred

#endif
