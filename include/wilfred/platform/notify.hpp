#pragma once

// Fire-and-forget user notification for background events (currently the
// due-reminder tick in the service loop). Must never block the caller:
// platform popups run on a detached worker thread. Strings are UTF-8.

#include <string>

namespace wilfred {

// Shows a system notification: PowerShell/WScript popup on Windows,
// osascript display notification on macOS, notify-send on Linux/BSD. Falls
// back to the log when no mechanism exists (mobile shells).
void notify_user(const std::string& title, const std::string& body);

}  // namespace wilfred
