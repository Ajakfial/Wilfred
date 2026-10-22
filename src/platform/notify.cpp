#include "wilfred/platform/notify.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/utf8.hpp"

#include <cstdio>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace wilfred {

namespace {

#ifdef _WIN32
// MessageBoxW pops in front of the user and stays until dismissed; it needs
// to pump its own modal loop, so it lives on a dedicated throwaway thread.
// The daemon process keeps running the whole time.
void popup_thread(std::wstring title, std::wstring body) {
  MessageBoxW(nullptr, body.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
}
#endif

#if !defined(_WIN32)
// POSIX: escape for single quotes, the only character raw '...' strings are
// sensitive to, and drop control bytes that could bend the command line.
std::string shell_quote(const std::string& s) {
  std::string out = "'";
  for (char c : s) {
    auto u = static_cast<unsigned char>(c);
    if (c == '\'')
      out += "'\\''";
    else if (c < 0x20 || c == 0x7f)
      out += ' ';
    else
      out.push_back(c);
  }
  out += "'";
  return out;
}
#endif

}  // namespace

void notify_user(const std::string& title, const std::string& body) {
#ifdef WILFRED_IOS
  (void)title;
  (void)body;
#elif defined(__APPLE__)
  // Notification Center banners appear even when Wilfred has no window.
  std::string cmd = "osascript -e 'display notification " + shell_quote(body) + " with title " +
                    shell_quote(title) + "' >/dev/null 2>&1";
  std::thread([cmd] { std::system(cmd.c_str()); }).detach();
#elif defined(__linux__) || defined(WILFRED_BSD)
  std::string cmd = "notify-send --expire-time=15000 " + shell_quote(title) + " " +
                    shell_quote(body) + " >/dev/null 2>&1";
  std::thread([cmd] { std::system(cmd.c_str()); }).detach();
#elif defined(_WIN32)
  std::thread([t = utf8_to_wide(title), b = utf8_to_wide(body)] {
    popup_thread(std::move(t), std::move(b));
  }).detach();
#else
  // Android and other shells: log only; the Kotlin layer owns toasts.
  (void)title;
  (void)body;
#endif
}

}  // namespace wilfred
