#include "wilfred/platform/native.hpp"

#include <cstdlib>
#include <string>

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)

static bool sys(const std::string& cmd) { return std::system(cmd.c_str()) == 0; }

bool native_launch(const std::string& path) {
  std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_reveal(const std::string& path) {
  std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_open_url(const std::string& url) {
  std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
  return sys(cmd);
}

bool native_system_action(const std::string& id) {
  if (id == "lock") {
    return sys("loginctl lock-session >/dev/null 2>&1") ||
           sys("xdg-screensaver lock >/dev/null 2>&1") ||
           sys("gnome-screensaver-command -l >/dev/null 2>&1") ||
           sys("dm-tool lock >/dev/null 2>&1");
  }
  if (id == "sleep") return sys("systemctl suspend >/dev/null 2>&1 &") ||
                            sys("loginctl suspend >/dev/null 2>&1 &");
  if (id == "shutdown") return sys("systemctl poweroff >/dev/null 2>&1 &");
  if (id == "restart") return sys("systemctl reboot >/dev/null 2>&1 &");
  if (id == "logout")
    return sys("loginctl terminate-session \"$XDG_SESSION_ID\" >/dev/null 2>&1 &") ||
           sys("gnome-session-quit --logout --no-prompt >/dev/null 2>&1 &");
  if (id == "empty_trash")
    return sys("gio trash --empty >/dev/null 2>&1") ||
           sys("rm -rf \"$HOME/.local/share/Trash/files/\"* \"$HOME/.local/share/Trash/info/\"* "
               ">/dev/null 2>&1");
  return false;
}

#endif
}  // namespace wilfred
