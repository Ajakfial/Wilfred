#include "wilfred/platform/native.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/utf8.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)

static std::string run_cmd(const char* cmd) {
  FILE* f = popen(cmd, "r");
  if (!f) return {};
  char buf[1024];
  std::string o;
  while (fgets(buf, sizeof(buf), f)) o += buf;
  pclose(f);
  while (!o.empty() && (o.back() == '\n' || o.back() == '\r')) o.pop_back();
  return o;
}

std::string native_default_browser_id() {
  auto s = run_cmd("xdg-settings get default-web-browser 2>/dev/null");
  auto l = to_lower_utf8(s);
  if (l.find("chrome") != std::string::npos) return "chrome";
  if (l.find("firefox") != std::string::npos) return "firefox";
  if (l.find("chromium") != std::string::npos) return "chromium";
  if (l.find("brave") != std::string::npos) return "brave";
  if (!s.empty()) return s;
  return "xdg-open";
}

std::string native_default_browser_executable() {
  auto id = native_default_browser_id();
  if (id.find(".desktop") != std::string::npos) {
    auto exe = run_cmd(("grep -m1 '^Exec=' /usr/share/applications/" + id +
                        " 2>/dev/null | cut -d= -f2 | awk '{print $1}'")
                           .c_str());
    if (!exe.empty()) return exe;
  }
  return id;
}

std::vector<BrowserInfo> native_list_browsers() {
  std::vector<BrowserInfo> out;
  const char* names[][2] = {{"firefox", "Mozilla Firefox"},
                            {"google-chrome", "Google Chrome"},
                            {"chromium", "Chromium"},
                            {"brave-browser", "Brave"},
                            {nullptr, nullptr}};
  auto def = native_default_browser_id();
  for (int i = 0; names[i][0]; ++i) {
    auto which = run_cmd((std::string("command -v ") + names[i][0] + " 2>/dev/null").c_str());
    if (which.empty()) continue;
    BrowserInfo b;
    b.id = names[i][0];
    b.name = names[i][1];
    b.executable = which;
    b.is_default = def.find(b.id) != std::string::npos;
    out.push_back(std::move(b));
  }
  if (out.empty()) {
    BrowserInfo b;
    b.id = def;
    b.name = def;
    b.executable = "xdg-open";
    b.is_default = true;
    out.push_back(b);
  }
  return out;
}

#endif
}  // namespace wilfred
