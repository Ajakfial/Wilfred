#include "wilfred/platform/platform.hpp"
#include "wilfred/ui/overlay.hpp"

#ifdef _WIN32
// windows.h first: objbase.h builds on its COM types.
#include <windows.h>

#include <objbase.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace wilfred {

std::string platform_name() {
#ifdef __ANDROID__
  return "android";
#elif defined(WILFRED_IOS)
  return "ios";
#elif defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#elif defined(__FreeBSD__)
  return "freebsd";
#elif defined(__OpenBSD__)
  return "openbsd";
#elif defined(__NetBSD__)
  return "netbsd";
#elif defined(__DragonFly__)
  return "dragonfly";
#else
  return "linux";
#endif
}

bool is_windows() {
#ifdef _WIN32
  return true;
#else
  return false;
#endif
}
bool is_macos() {
#if defined(__APPLE__) && !defined(WILFRED_IOS)
  return true;
#else
  return false;
#endif
}
bool is_linux() {
  return !is_windows() && !is_macos() && !is_ios() && !is_bsd();
}

bool is_android() {
#ifdef __ANDROID__
  return true;
#else
  return false;
#endif
}

bool is_ios() {
#ifdef WILFRED_IOS
  return true;
#else
  return false;
#endif
}

bool is_bsd() {
#ifdef WILFRED_BSD
  return true;
#else
  return false;
#endif
}

void platform_init() {
#ifdef _WIN32
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif
}

void pump_native_events() {
#ifdef _WIN32
  MSG msg;
  while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }
#elif defined(__APPLE__)
  CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
#endif
  overlay_pump();
}

// Pure content builders for the file-based autostart backends. Kept here
// (compiled on every platform) so tests can verify them without touching
// the machine's login items.

std::string autostart_desktop_entry(const std::string& exe) {
  return "[Desktop Entry]\n"
         "Type=Application\n"
         "Name=Wilfred\n"
         "Comment=Fast keyboard launcher and automation hub\n"
         "Exec=\"" +
         exe +
         "\"\n"
         "Hidden=false\n"
         "X-GNOME-Autostart-enabled=true\n";
}

std::string autostart_launchd_plist(const std::string& exe) {
  std::string esc;
  for (char c : exe) {
    if (c == '&')
      esc += "&amp;";
    else if (c == '<')
      esc += "&lt;";
    else if (c == '>')
      esc += "&gt;";
    else
      esc += c;
  }
  return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
         "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
         "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
         "<plist version=\"1.0\">\n"
         "<dict>\n"
         "  <key>Label</key>\n"
         "  <string>com.wilfred.launcher</string>\n"
         "  <key>ProgramArguments</key>\n"
         "  <array>\n"
         "    <string>" +
         esc +
         "</string>\n"
         "  </array>\n"
         "  <key>RunAtLoad</key>\n"
         "  <true/>\n"
         "</dict>\n"
         "</plist>\n";
}

}  // namespace wilfred
