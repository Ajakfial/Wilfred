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

std::string platform_name();
bool is_windows();
bool is_macos();
bool is_linux();
bool is_android();
bool is_ios();

void platform_init();
void pump_native_events();

}  // namespace wilfred
