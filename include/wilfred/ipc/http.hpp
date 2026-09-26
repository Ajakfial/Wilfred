#pragma once

#include "wilfred/ipc/protocol.hpp"

#include <functional>
#include <memory>
#include <string>

namespace wilfred {

struct HttpApiRequest {
  std::string method;
  std::string path;
  std::string query;
  std::string body;
  std::string token;
  std::string peer;
};

struct HttpApiResponse {
  int status{200};
  std::string content_type{"application/json"};
  std::string body;
};

using HttpApiHandler = std::function<HttpApiResponse(const HttpApiRequest&)>;

class HttpApiServer {
public:
  HttpApiServer();
  ~HttpApiServer();
  bool start(const std::string& bind, int port, const std::string& token, HttpApiHandler handler);
  void stop();
  int port() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

std::string http_get(const std::string& url, const std::string& token, int timeout_ms = 8000);
bool http_put(const std::string& url, const std::string& token, const std::string& body,
              int timeout_ms = 20000, std::string* err = nullptr);

}  // namespace wilfred
