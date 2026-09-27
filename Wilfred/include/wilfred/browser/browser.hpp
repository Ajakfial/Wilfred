#pragma once

#include <string>
#include <vector>

namespace wilfred {

struct BrowserInfo {
  std::string id;
  std::string name;
  std::string executable;
  bool is_default{false};
};

std::string default_browser_id();
std::string default_browser_executable();
std::vector<BrowserInfo> list_browsers();
std::string web_search_url(const std::string& tmpl, const std::string& query);
bool open_in_default_browser(const std::string& url);

}  // namespace wilfred
