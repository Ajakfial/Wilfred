#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace wilfred {

// Autostart via the XDG autostart dir (~/.config/autostart/wilfred.desktop),
// honored by GNOME/KDE/Xfce and most others. Bare `wilfred` runs the daemon,
// so Exec carries just the quoted exe path.
namespace {
std::string entry_path() {
  return path_join(home_directory(), ".config/autostart/wilfred.desktop");
}

bool read_file(const std::string& p, std::string& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

// Value of the first "Exec=" line, or empty.
std::string exec_line(const std::string& body) {
  std::string::size_type pos = 0;
  while (pos < body.size()) {
    std::string::size_type end = body.find('\n', pos);
    std::string line = body.substr(pos, end == std::string::npos ? end : end - pos);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.compare(0, 5, "Exec=") == 0) return line.substr(5);
    if (end == std::string::npos) break;
    pos = end + 1;
  }
  return "";
}
}  // namespace

bool native_autostart_status(bool& enabled, std::string& detail, std::string& error) {
  enabled = false;
  std::string path = entry_path();
  std::string body;
  if (!read_file(path, body)) {
    detail = path + " (absent)";
    return true;
  }
  std::string exe = exe_path();
  std::string want = exe.empty() ? std::string() : "\"" + exe + "\"";
  std::string got = exec_line(body);
  if (got.empty()) {
    detail = path + " (no Exec line)";
    return true;
  }
  enabled = !want.empty() && got == want;
  detail = enabled ? path : path + " (points elsewhere)";
  (void)error;
  return true;
}

bool native_autostart_set(bool enable, std::string& error) {
  std::string path = entry_path();
  if (!enable) {
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
      error = "cannot remove " + path + ": " + ec.message();
      return false;
    }
    return true;
  }
  std::string exe = exe_path();
  if (exe.empty()) {
    error = "cannot determine executable path";
    return false;
  }
  std::error_code ec;
  std::filesystem::create_directories(path_join(home_directory(), ".config/autostart"), ec);
  if (ec) {
    error = "cannot create autostart dir: " + ec.message();
    return false;
  }
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write " + path;
    return false;
  }
  f << autostart_desktop_entry(exe);
  f.flush();
  if (!f) {
    error = "cannot write " + path;
    return false;
  }
  return true;
}

}  // namespace wilfred
