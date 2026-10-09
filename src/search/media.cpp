#include "wilfred/search/media.hpp"

#include "wilfred/platform/platform.hpp"
#include "wilfred/search/mpris_dbus.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#ifdef _WIN32
// Implemented in src/platform/win/launch.cpp to keep Win32 includes in one place.
#elif defined(__ANDROID__) || defined(WILFRED_IOS)
// Implemented in src/platform/android/launch.cpp / src/platform/ios/launch.cpp
// as explicit-error stubs: process control and media keys are desktop-only
// on mobile.
#else
#include <signal.h>
#include <sys/types.h>

namespace wilfred {

bool native_kill_process(std::uint32_t pid, std::string& error) {
  if (!pid || pid == 1) {
    error = "invalid pid";
    return false;
  }
  if (::kill(static_cast<pid_t>(pid), SIGTERM) == 0) return true;
  // Fall back to SIGKILL when the process ignores TERM.
  if (::kill(static_cast<pid_t>(pid), 0) == 0) {
    if (::kill(static_cast<pid_t>(pid), SIGKILL) == 0) return true;
  }
  error = "could not signal pid " + std::to_string(pid);
  return false;
}

#ifndef __APPLE__

namespace {

bool have_tool(const char* name) {
  std::string cmd = std::string("command -v ") + name + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

bool sys_ok(const std::string& cmd) {
  return std::system(cmd.c_str()) == 0;
}

bool playerctl_try(const std::string& id) {
  std::string arg = id == "playpause" ? "play-pause" : id;
  if (id == "play") arg = "play";
  if (id == "pause") arg = "pause";
  return sys_ok("playerctl " + arg + " >/dev/null 2>&1");
}

// PipeWire / Pulse / ALSA volume chain. Tries whatever the system already
// ships — no new installs required. Order: wpctl (PipeWire native) ->
// pactl (Pulse/PipeWire-pulse) -> amixer (ALSA) -> pamixer (optional).
bool volume_try(const std::string& id, bool mute) {
  if (mute) {
    if (have_tool("wpctl") && sys_ok("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle >/dev/null 2>&1"))
      return true;
    if (have_tool("pactl") && sys_ok("pactl set-sink-mute @DEFAULT_SINK@ toggle >/dev/null 2>&1"))
      return true;
    if (have_tool("amixer") && sys_ok("amixer -q sset Master toggle >/dev/null 2>&1")) return true;
    if (have_tool("pamixer") && sys_ok("pamixer -t >/dev/null 2>&1")) return true;
    return false;
  }
  std::string wp = id == "volup" ? "10%+" : "10%-";
  std::string pa = id == "volup" ? "+10%" : "-10%";
  std::string al = id == "volup" ? "10%+" : "10%-";
  if (have_tool("wpctl") &&
      sys_ok("wpctl set-volume @DEFAULT_AUDIO_SINK@ " + wp + " >/dev/null 2>&1"))
    return true;
  if (have_tool("pactl") &&
      sys_ok("pactl set-sink-volume @DEFAULT_SINK@ " + pa + " >/dev/null 2>&1"))
    return true;
  if (have_tool("amixer") && sys_ok("amixer -q sset Master " + al + " >/dev/null 2>&1"))
    return true;
  if (have_tool("pamixer")) {
    if (sys_ok(std::string("pamixer ") + (id == "volup" ? "-i 10" : "-d 10") + " >/dev/null 2>&1"))
      return true;
  }
  return false;
}

}  // namespace

bool native_media_action(const std::string& id, std::string& error) {
  if (id == "play" || id == "pause" || id == "playpause" || id == "next" || id == "prev" ||
      id == "stop") {
    // Genuine path first: native MPRIS over D-Bus, no helpers needed.
    // Covers Spotify, VLC, Chrome/Chromium, Firefox, mpv, mpd, etc. —
    // anything exposing org.mpris.MediaPlayer2 on the session bus.
    std::string native_err;
    if (mpris_media_action(id, native_err)) return true;
    // Optional enhancement when installed; never required.
    if (have_tool("playerctl") && playerctl_try(id)) return true;
    if (have_tool("playerctl"))
      error = "playback failed (tried native MPRIS D-Bus + playerctl: " + native_err + ")";
    else if (!native_err.empty())
      error = native_err + " (tried native MPRIS D-Bus; playerctl not installed, none required)";
    else
      error = "no MPRIS players found (tried native D-Bus)";
    return false;
  }
  if (id == "mute") {
    if (volume_try(id, true)) return true;
    error = "mute failed (tried wpctl, pactl, amixer, pamixer — none found/working)";
    return false;
  }
  if (id == "volup" || id == "voldn") {
    if (volume_try(id, false)) return true;
    error = "volume change failed (tried wpctl, pactl, amixer, pamixer — none found/working)";
    return false;
  }
  error = "unknown media action '" + id + "'";
  return false;
}

#endif  // !__APPLE__

}  // namespace wilfred
#endif
