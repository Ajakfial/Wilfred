#pragma once

#include "wilfred/index/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct AppInfo {
  std::string name;
  std::string path;
  std::string identifier;
  std::vector<std::string> keywords;
  bool system{false};
};

std::vector<AppInfo> discover_applications();
void index_applications(IndexEngine& engine);

bool launch_path(const std::string& path);
bool reveal_path(const std::string& path);
bool open_url(const std::string& url);

}  // namespace wilfred
