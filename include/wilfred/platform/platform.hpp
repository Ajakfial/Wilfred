#pragma once

#include <string>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

namespace wilfred {

// iOS (iPhone/iPad) is Apple Silicon/ARM like macOS but sandboxed like
// Android: no overlay UI, no app enumeration, no global hotkeys. iOS gets
// its own platform backend (src/platform/ios/) and Swift UI (ios/);
// macOS desktop code paths must therefore test WILFRED_IOS first.
#if defined(__APPLE__) && defined(TARGET_OS_IOS) && TARGET_OS_IOS
#define WILFRED_IOS 1
#endif

// BSD desktops (FreeBSD/OpenBSD/NetBSD/DragonFly) are Unix-like but lack
// Linux specifics: no inotify (kqueue instead), no /proc, sysctl-based
// system info. They get a native backend (src/platform/bsd/) reusing the
// portable XDG/X11 Linux files where the APIs match.
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || \
    defined(__DragonFly__)
#define WILFRED_BSD 1
#endif

std::string platform_name();
bool is_windows();
bool is_macos();
bool is_linux();
bool is_android();
bool is_ios();
bool is_bsd();

void platform_init();
void pump_native_events();

}  // namespace wilfred
