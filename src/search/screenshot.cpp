#include "wilfred/search/screenshot.hpp"

#include "wilfred/core/utf8.hpp"
#include "wilfred/platform/native.hpp"

namespace wilfred {

std::string screenshot_mode_name(ScreenshotMode mode) {
  switch (mode) {
    case ScreenshotMode::Fullscreen:
      return "fullscreen";
    case ScreenshotMode::Window:
      return "window";
    case ScreenshotMode::Region:
      return "region";
  }
  return "fullscreen";
}

bool parse_screenshot_mode(const std::string& s, ScreenshotMode& out) {
  auto l = to_lower_utf8(s);
  // Trim whitespace.
  while (!l.empty() && (l.front() == ' ' || l.front() == '\t')) l.erase(l.begin());
  while (!l.empty() && (l.back() == ' ' || l.back() == '\t')) l.pop_back();
  if (l.empty()) return false;
  if (l == "fullscreen" || l == "full" || l == "screen" || l == "desktop" || l == "all" ||
      l == "monitor" || l == "monitors")
    out = ScreenshotMode::Fullscreen;
  else if (l == "window" || l == "win" || l == "active" || l == "activewindow" ||
           l == "active-window")
    out = ScreenshotMode::Window;
  else if (l == "region" || l == "selection" || l == "select" || l == "area" || l == "crop" ||
           l == "partial" || l == "rect" || l == "rectangle")
    out = ScreenshotMode::Region;
  else
    return false;
  return true;
}

std::string screenshot_payload(ScreenshotMode mode) {
  return "screenshot:" + screenshot_mode_name(mode);
}

bool parse_screenshot_payload(const std::string& payload, ScreenshotMode& out) {
  auto l = to_lower_utf8(payload);
  const std::string prefix = "screenshot:";
  auto pos = l.find(prefix);
  if (pos == std::string::npos) return false;
  return parse_screenshot_mode(payload.substr(pos + prefix.size()), out);
}

bool take_screenshot(ScreenshotMode mode, std::string& out_path, std::string& error) {
  NativeScreenshotMode nm = NativeScreenshotMode::Fullscreen;
  switch (mode) {
    case ScreenshotMode::Fullscreen:
      nm = NativeScreenshotMode::Fullscreen;
      break;
    case ScreenshotMode::Window:
      nm = NativeScreenshotMode::Window;
      break;
    case ScreenshotMode::Region:
      nm = NativeScreenshotMode::Region;
      break;
  }
  return native_take_screenshot(nm, out_path, error);
}

}  // namespace wilfred
