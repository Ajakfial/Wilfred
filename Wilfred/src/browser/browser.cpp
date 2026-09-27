#include "wilfred/browser/browser.hpp"
#include "wilfred/platform/native.hpp"

#include <cctype>
#include <cstdio>
#include <sstream>

namespace wilfred {

std::string default_browser_id() { return native_default_browser_id(); }
std::string default_browser_executable() { return native_default_browser_executable(); }
std::vector<BrowserInfo> list_browsers() { return native_list_browsers(); }

std::string web_search_url(const std::string& tmpl, const std::string& query) {
  std::string enc;
  enc.reserve(query.size() * 3);
  for (unsigned char c : query) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
      enc.push_back(static_cast<char>(c));
    else if (c == ' ')
      enc.push_back('+');
    else {
      char buf[8];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      enc += buf;
    }
  }
  std::string out = tmpl;
  auto pos = out.find("{query}");
  if (pos != std::string::npos) out.replace(pos, 7, enc);
  return out;
}

bool open_in_default_browser(const std::string& url) { return native_open_url(url); }

}  // namespace wilfred
