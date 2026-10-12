#include "wilfred/platform/native.hpp"

#include "wilfred/core/paths.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace wilfred {

// Autostart via ~/Library/LaunchAgents/com.wilfred.launcher.plist
// (RunAtLoad). No ObjC needed: the agent entry is a plain file write.
namespace {
std::string plist_path() {
  return path_join(home_directory(), "Library/LaunchAgents/com.wilfred.launcher.plist");
}

bool read_file(const std::string& p, std::string& out) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}
}  // namespace

bool native_autostart_status(bool& enabled, std::string& detail, std::string& error) {
  enabled = false;
  std::string path = plist_path();
  std::string body;
  if (!read_file(path, body)) {
    detail = path + " (absent)";
    return true;
  }
  if (body.find("<string>com.wilfred.launcher</string>") == std::string::npos) {
    detail = path + " (foreign entry)";
    return true;
  }
  std::string exe = exe_path();
  enabled = !exe.empty() && body.find("<string>" + exe + "</string>") != std::string::npos;
  detail = enabled ? path : path + " (points elsewhere)";
  (void)error;
  return true;
}

bool native_autostart_set(bool enable, std::string& error) {
  std::string path = plist_path();
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
  std::filesystem::create_directories(path_join(home_directory(), "Library/LaunchAgents"), ec);
  if (ec) {
    error = "cannot create LaunchAgents dir: " + ec.message();
    return false;
  }
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) {
    error = "cannot write " + path;
    return false;
  }
  f << autostart_launchd_plist(exe);
  f.flush();
  if (!f) {
    error = "cannot write " + path;
    return false;
  }
  return true;
}

}  // namespace wilfred
