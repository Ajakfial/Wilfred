#pragma once

#include "wilfred/ipc/protocol.hpp"

#include <functional>
#include <memory>
#include <string>

namespace wilfred {

using IpcHandler = std::function<IpcResponse(const IpcRequest&)>;

class IpcServer {
public:
  IpcServer();
  ~IpcServer();
  bool start(const std::string& endpoint, IpcHandler handler);
  void stop();
  static std::string send(const std::string& endpoint, const std::string& line);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace wilfred
