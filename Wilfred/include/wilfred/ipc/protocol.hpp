#pragma once

#include "wilfred/search/engine.hpp"

#include <optional>
#include <string>
#include <vector>

namespace wilfred {

struct IpcRequest {
  std::string cmd;
  std::string query;
  std::string path;
  std::string action;
  std::string token;
  int limit{40};
};

struct IpcResponse {
  bool ok{true};
  std::string error;
  std::string text;
  std::vector<SearchResult> results;
};

std::string encode_response(const IpcResponse& r);
bool decode_request(const std::string& line, IpcRequest& out);

}  // namespace wilfred
