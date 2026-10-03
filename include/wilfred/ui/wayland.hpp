#pragma once

#include <cstdlib>
#include <string>

namespace wilfred {

// Wayland session detection for the Linux overlay backend. True when the
// process runs inside a Wayland session: WAYLAND_DISPLAY is set
// (non-empty), or XDG_SESSION_TYPE is exactly "wayland". Pure stdlib, so it
// stays unit testable without Wayland headers or a running compositor.
//
// Used to decide whether the WebKitGTK overlay window should become a
// layer-shell surface (always-on-top + top anchoring + exclusive keyboard,
// which plain GTK hints cannot do on Wayland). gtk-layer-shell itself
// ignores compositors without the protocol, so this is only a fast path.
inline bool wayland_session_hint() {
  const char* display = std::getenv("WAYLAND_DISPLAY");
  if (display != nullptr && display[0] != '\0') return true;
  const char* session = std::getenv("XDG_SESSION_TYPE");
  return session != nullptr && std::string(session) == "wayland";
}

}  // namespace wilfred
