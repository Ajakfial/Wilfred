#include "wilfred/platform/platform.hpp"
#include "wilfred/ui/overlay.hpp"

#ifdef _WIN32
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
bool is_linux() { return !is_windows() && !is_macos() && !is_ios() && !is_bsd(); }

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

}  // namespace wilfred
