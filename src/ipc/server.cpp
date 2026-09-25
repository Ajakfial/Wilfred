#include "wilfred/ipc/server.hpp"

#include "wilfred/core/log.hpp"

#include <atomic>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace wilfred {

struct IpcServer::Impl {
  std::string endpoint;
  IpcHandler handler;
  std::atomic<bool> running{false};
  std::thread th;
#ifdef _WIN32
  HANDLE stop_event{nullptr};
#else
  int listen_fd{-1};
#endif

  void loop();
};

IpcServer::IpcServer() : impl_(std::make_unique<Impl>()) {}
IpcServer::~IpcServer() { stop(); }

#ifdef _WIN32
void IpcServer::Impl::loop() {
  while (running) {
    HANDLE pipe = CreateNamedPipeA(endpoint.c_str(), PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                   PIPE_UNLIMITED_INSTANCES, 65536, 65536, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      Sleep(50);
      continue;
    }
    BOOL ok = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
    if (!ok) {
      CloseHandle(pipe);
      continue;
    }
    char buf[8192];
    DWORD n = 0;
    if (ReadFile(pipe, buf, sizeof(buf) - 1, &n, nullptr) && n > 0) {
      buf[n] = 0;
      std::string line(buf, n);
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
      IpcRequest req;
      IpcResponse resp;
      if (!decode_request(line, req)) {
        resp.ok = false;
        resp.error = "invalid request";
      } else {
        try {
          resp = handler(req);
        } catch (...) {
          resp.ok = false;
          resp.error = "handler failed";
        }
      }
      auto out = encode_response(resp);
      DWORD wr = 0;
      WriteFile(pipe, out.data(), static_cast<DWORD>(out.size()), &wr, nullptr);
    }
    DisconnectNamedPipe(pipe);
    CloseHandle(pipe);
  }
}
#else
void IpcServer::Impl::loop() {
  while (running) {
    sockaddr_un addr{};
    socklen_t len = sizeof(addr);
    int c = accept(listen_fd, reinterpret_cast<sockaddr*>(&addr), &len);
    if (c < 0) {
      if (!running) break;
      continue;
    }
    char buf[8192];
    auto n = ::read(c, buf, sizeof(buf) - 1);
    if (n > 0) {
      buf[n] = 0;
      std::string line(buf, static_cast<std::size_t>(n));
      while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
      IpcRequest req;
      IpcResponse resp;
      if (!decode_request(line, req)) {
        resp.ok = false;
        resp.error = "invalid request";
      } else {
        try {
          resp = handler(req);
        } catch (...) {
          resp.ok = false;
          resp.error = "handler failed";
        }
      }
      auto out = encode_response(resp);
      ::write(c, out.data(), out.size());
    }
    ::close(c);
  }
}
#endif

bool IpcServer::start(const std::string& endpoint, IpcHandler handler) {
  impl_->endpoint = endpoint;
  impl_->handler = std::move(handler);
  impl_->running = true;
#ifndef _WIN32
  unlink(endpoint.c_str());
  impl_->listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (impl_->listen_fd < 0) return false;
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", endpoint.c_str());
  if (bind(impl_->listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
  if (listen(impl_->listen_fd, 16) != 0) return false;
#endif
  impl_->th = std::thread([this] { impl_->loop(); });
  return true;
}

void IpcServer::stop() {
  if (!impl_) return;
  impl_->running = false;
#ifdef _WIN32
  // Unblock ConnectNamedPipe by opening the pipe once.
  HANDLE h = CreateFileA(impl_->endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                         OPEN_EXISTING, 0, nullptr);
  if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
#else
  if (impl_->listen_fd >= 0) {
    shutdown(impl_->listen_fd, SHUT_RDWR);
    close(impl_->listen_fd);
    impl_->listen_fd = -1;
  }
  unlink(impl_->endpoint.c_str());
#endif
  if (impl_->th.joinable()) impl_->th.join();
}

std::string IpcServer::send(const std::string& endpoint, const std::string& line) {
#ifdef _WIN32
  HANDLE h = CreateFileA(endpoint.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                         0, nullptr);
  if (h == INVALID_HANDLE_VALUE) return {};
  DWORD wr = 0;
  WriteFile(h, line.data(), static_cast<DWORD>(line.size()), &wr, nullptr);
  char buf[65536];
  DWORD n = 0;
  ReadFile(h, buf, sizeof(buf) - 1, &n, nullptr);
  CloseHandle(h);
  return n ? std::string(buf, n) : std::string{};
#else
  int fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (fd < 0) return {};
  sockaddr_un addr{};
  addr.sun_family = AF_UNIX;
  std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", endpoint.c_str());
  if (connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close(fd);
    return {};
  }
  ::write(fd, line.data(), line.size());
  char buf[65536];
  auto n = ::read(fd, buf, sizeof(buf) - 1);
  close(fd);
  return n > 0 ? std::string(buf, static_cast<std::size_t>(n)) : std::string{};
#endif
}

}  // namespace wilfred
