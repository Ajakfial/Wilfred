#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreServices/CoreServices.h>
#import <IOKit/graphics/IOGraphicsLib.h>

namespace wilfred {
#ifdef __APPLE__

namespace {

std::string mac_shell_quote(const std::string& s) {
  std::string o = "\"";
  for (char c : s) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') o.push_back('\\');
    o.push_back(c);
  }
  o.push_back('"');
  return o;
}

}  // namespace

bool native_launch(const std::string& path) {
  @autoreleasepool {
    NSString* s = [NSString stringWithUTF8String:path.c_str()];
    return [[NSWorkspace sharedWorkspace] openFile:s];
  }
}

bool native_reveal(const std::string& path) {
  @autoreleasepool {
    NSString* s = [NSString stringWithUTF8String:path.c_str()];
    NSURL* url = [NSURL fileURLWithPath:s];
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
    return true;
  }
}

bool native_open_url(const std::string& url) {
  @autoreleasepool {
    NSURL* u = [NSURL URLWithString:[NSString stringWithUTF8String:url.c_str()]];
    if (!u) return false;
    return [[NSWorkspace sharedWorkspace] openURL:u];
  }
}

bool native_system_action(const std::string& id) {
  @autoreleasepool {
    NSString* script = nil;
    if (id == "lock") {
      if (std::system("open -a ScreenSaverEngine >/dev/null 2>&1") == 0) return true;
      script = @"tell application \"System Events\" to keystroke \"q\" using {command down, "
               @"control down}";
    } else if (id == "sleep") {
      return std::system("pmset sleepnow >/dev/null 2>&1") == 0;
    } else if (id == "shutdown") {
      script = @"tell application \"System Events\" to shut down";
    } else if (id == "restart") {
      script = @"tell application \"System Events\" to restart";
    } else if (id == "logout") {
      script = @"tell application \"System Events\" to log out";
    } else if (id == "empty_trash") {
      script = @"tell application \"Finder\" to empty the trash";
    } else {
      return false;
    }
    NSDictionary* err = nil;
    NSAppleScript* as = [[NSAppleScript alloc] initWithSource:script];
    NSAppleEventDescriptor* r = [as executeAndReturnError:&err];
    return r != nil && err == nil;
  }
}

std::vector<OpenWithApp> native_apps_for_file(const std::string& path, std::size_t max_apps) {
  std::vector<OpenWithApp> out;
  if (max_apps == 0 || path.empty()) return out;
  @autoreleasepool {
    NSString* s = [NSString stringWithUTF8String:path.c_str()];
    if (!s) return out;
    NSURL* url = [NSURL fileURLWithPath:s];
    if (!url) return out;
    CFArrayRef apps = LSCopyApplicationURLsForURL((__bridge CFURLRef)url, kLSRolesAll);
    if (!apps) return out;
    NSFileManager* fm = [NSFileManager defaultManager];
    CFIndex n = CFArrayGetCount(apps);
    for (CFIndex i = 0; i < n && out.size() < max_apps; ++i) {
      NSURL* app = (__bridge NSURL*)CFArrayGetValueAtIndex(apps, i);
      NSString* bundle = app.path;
      if (!bundle) continue;
      // Skip Wilfred itself so it never suggests opening files with the launcher.
      if ([bundle rangeOfString:@"Wilfred"].location != NSNotFound) continue;
      NSString* name = [fm displayNameAtPath:bundle];
      if (!name || !name.UTF8String || !*name.UTF8String) continue;
      bool dup = false;
      for (auto& e : out)
        if (e.name == name.UTF8String) {
          dup = true;
          break;
        }
      if (dup) continue;
      OpenWithApp a;
      a.name = name.UTF8String;
      a.target = bundle.UTF8String ? bundle.UTF8String : "";
      if (a.target.empty()) continue;
      out.push_back(std::move(a));
    }
    CFRelease(apps);
  }
  return out;
}

bool native_open_with(const std::string& target, const std::string& file) {
  if (target.empty() || file.empty()) return false;
  std::string cmd =
      "open -a " + mac_shell_quote(target) + " " + mac_shell_quote(file) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

bool native_open_terminal(const std::string& dir) {
  if (dir.empty()) return false;
  std::string cmd = "open -a Terminal " + mac_shell_quote(dir) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

bool native_open_editor(const std::string& dir) {
  if (dir.empty()) return false;
  @autoreleasepool {
    NSFileManager* fm = [NSFileManager defaultManager];
    if ([fm fileExistsAtPath:@"/Applications/Visual Studio Code.app"]) {
      std::string cmd = "open -a " + mac_shell_quote("Visual Studio Code") + " " +
                        mac_shell_quote(dir) + " >/dev/null 2>&1";
      if (std::system(cmd.c_str()) == 0) return true;
    }
  }
  if (std::system("command -v code >/dev/null 2>&1") == 0) {
    std::string cmd = "code -n " + mac_shell_quote(dir) + " >/dev/null 2>&1 &";
    if (std::system(cmd.c_str()) == 0) return true;
  }
  return native_launch(dir);
}

// Genuine system-wide media keys via HID (no helper apps needed).
// NX_KEYTYPE values from IOKit/hidsystem/ev_keymap.h (hardcoded to avoid a
// new framework dependency; AppKit/CoreGraphics already linked):
// PLAY=16, NEXT=17, PREVIOUS=18.
static bool PostMediaHID(int nxKeyType) {
  @autoreleasepool {
    // subtype 8 = media keys; data1 = (keyCode << 16) | (0xa down / 0xb up << 8).
    NSEvent* down = [NSEvent otherEventWithType:NSSystemDefined
                                       location:NSZeroPoint
                                  modifierFlags:0xa00
                                      timestamp:0
                                   windowNumber:0
                                        context:nil
                                        subtype:8
                                          data1:((nxKeyType << 16) | (0xa << 8))
                                          data2:-1];
    if (down) CGEventPost(kCGHIDEventTap, [down CGEvent]);
    NSEvent* up = [NSEvent otherEventWithType:NSSystemDefined
                                     location:NSZeroPoint
                                modifierFlags:0xa00
                                    timestamp:0
                                 windowNumber:0
                                      context:nil
                                      subtype:8
                                        data1:((nxKeyType << 16) | (0xb << 8))
                                        data2:-1];
    if (up) CGEventPost(kCGHIDEventTap, [up CGEvent]);
    return down != nil && up != nil;
  }
}

static bool mac_osascript_ok(const std::string& script) {
  std::string cmd = "osascript -e " + mac_shell_quote(script) + " >/dev/null 2>&1";
  return std::system(cmd.c_str()) == 0;
}

static std::string mac_exec(const std::string& cmd) {
  std::string full = cmd + " 2>/dev/null";
  FILE* f = popen(full.c_str(), "r");
  if (!f) return {};
  std::string out;
  char buf[512];
  while (fgets(buf, sizeof(buf), f)) {
    out += buf;
    if (out.size() > 16384) break;
  }
  pclose(f);
  return out;
}

static std::string mac_lower(std::string s) {
  for (auto& c : s)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

static std::string mac_wifi_dev() {
  auto out = mac_exec("networksetup -listallhardwareports");
  std::string cur;
  bool want = false;
  for (char c : out + "\n") {
    if (c == '\n') {
      auto t = cur;
      while (!t.empty() && (t.front() == ' ' || t.front() == '\t'))
        t.erase(t.begin());
      auto low = mac_lower(t);
      if (low.rfind("hardware port:", 0) == 0) {
        want = low.find("wi-fi") != std::string::npos || low.find("wifi") != std::string::npos ||
               low.find("airport") != std::string::npos;
      } else if (want && low.rfind("device:", 0) == 0) {
        auto v = t.substr(7);
        while (!v.empty() && (v.front() == ' ' || v.front() == '\t'))
          v.erase(v.begin());
        while (!v.empty() && (v.back() == ' ' || v.back() == '\r' || v.back() == '\t'))
          v.pop_back();
        if (!v.empty()) return v;
        want = false;
      }
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  return "en0";
}

bool native_media_action(const std::string& id, std::string& error) {
  // System-wide HID first — works with any player (Music, Spotify, VLC,
  // IINA, Chrome, Safari, …) with nothing to install.
  if (id == "play" || id == "pause" || id == "playpause") {
    if (PostMediaHID(16)) return true;
  } else if (id == "next") {
    if (PostMediaHID(17)) return true;
  } else if (id == "prev") {
    if (PostMediaHID(18)) return true;
  }
  // Fallback: per-app AppleScript for the big three when HID is blocked
  // (e.g. accessibility permissions) or for Stop (no HID code).
  if (id == "play" || id == "pause" || id == "playpause") {
    if (mac_osascript_ok("tell application \"Music\" to playpause")) return true;
    if (mac_osascript_ok("tell application \"Spotify\" to playpause")) return true;
    if (mac_osascript_ok("tell application \"VLC\" to play")) return true;
    error = "no controllable player found (tried system media keys + Music/Spotify/VLC)";
    return false;
  }
  if (id == "next") {
    if (mac_osascript_ok("tell application \"Music\" to next track")) return true;
    if (mac_osascript_ok("tell application \"Spotify\" to next track")) return true;
    if (mac_osascript_ok("tell application \"VLC\" to next")) return true;
    error = "no controllable player found (tried system media keys + Music/Spotify/VLC)";
    return false;
  }
  if (id == "prev") {
    if (mac_osascript_ok("tell application \"Music\" to previous track")) return true;
    if (mac_osascript_ok("tell application \"Spotify\" to previous track")) return true;
    if (mac_osascript_ok("tell application \"VLC\" to previous")) return true;
    error = "no controllable player found (tried system media keys + Music/Spotify/VLC)";
    return false;
  }
  if (id == "stop") {
    if (mac_osascript_ok("tell application \"Music\" to stop")) return true;
    if (mac_osascript_ok("tell application \"Spotify\" to pause")) return true;
    error = "stop failed (tried Music/Spotify)";
    return false;
  }
  if (id == "mute") {
    if (mac_osascript_ok("set volume output muted not (output muted of (get volume settings))"))
      return true;
    error = "mute failed";
    return false;
  }
  if (id == "volup" || id == "voldn") {
    std::string op = id == "volup" ? "+" : "-";
    if (mac_osascript_ok("set volume output volume ((output volume of (get volume settings)) " +
                         op + " 10)"))
      return true;
    error = "volume change failed";
    return false;
  }
  error = "unknown media action '" + id + "'";
  return false;
}

bool native_wifi_status(bool& enabled, std::string& detail, std::string& error) {
  auto dev = mac_wifi_dev();
  auto out = mac_exec("networksetup -getairportpower " + dev);
  auto low = mac_lower(out);
  if (low.find("on") != std::string::npos && low.find("off") == std::string::npos) {
    enabled = true;
  } else if (low.find("off") != std::string::npos) {
    enabled = false;
  } else {
    error = "cannot read Wi-Fi power (" + dev + ")";
    return false;
  }
  auto ssid = mac_exec("networksetup -getairportnetwork " + dev);
  // "Current Wi-Fi Network: MyNet" or "You are not associated...".
  std::string s = ssid;
  while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
    s.pop_back();
  auto p = s.find(':');
  std::string net;
  if (p != std::string::npos) {
    net = s.substr(p + 1);
    while (!net.empty() && (net.front() == ' ' || net.front() == '\t'))
      net.erase(net.begin());
  }
  detail = dev;
  if (!net.empty() && mac_lower(net).find("not associated") == std::string::npos)
    detail += " · " + net;
  return true;
}

bool native_wifi_set(bool enabled, std::string& error) {
  auto dev = mac_wifi_dev();
  std::string cmd =
      std::string("networksetup -setairportpower ") + dev + (enabled ? " on" : " off");
  if (std::system((cmd + " >/dev/null 2>&1").c_str()) != 0) {
    error = "networksetup failed (may need admin)";
    return false;
  }
  bool cur = false;
  std::string detail;
  if (!native_wifi_status(cur, detail, error)) return false;
  if (cur != enabled) {
    error = "Wi-Fi change did not apply";
    return false;
  }
  return true;
}

std::vector<std::string> native_wifi_list(std::string& error) {
  std::vector<std::string> out;
  auto dev = mac_wifi_dev();
  auto txt = mac_exec("networksetup -listpreferredwirelessnetworks " + dev);
  std::string cur;
  for (char c : txt + "\n") {
    if (c == '\n') {
      auto t = cur;
      while (!t.empty() && (t.front() == ' ' || t.front() == '\t'))
        t.erase(t.begin());
      while (!t.empty() && (t.back() == ' ' || t.back() == '\r' || t.back() == '\t'))
        t.pop_back();
      if (!t.empty() && t[0] != '-' && mac_lower(t).find("preferred") == std::string::npos)
        out.push_back(t);
      if (out.size() >= 8) break;
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (out.empty()) error = "no preferred networks listed";
  return out;
}

bool native_bluetooth_status(bool& enabled, std::string& detail, std::string& error) {
  if (std::system("command -v blueutil >/dev/null 2>&1") == 0) {
    auto out = mac_exec("blueutil --power");
    auto low = mac_lower(out);
    while (!low.empty() && (low.back() == '\n' || low.back() == '\r' || low.back() == ' '))
      low.pop_back();
    if (low == "1" || low.find("on") != std::string::npos) {
      enabled = true;
      detail = "blueutil";
      return true;
    }
    if (low == "0" || low.find("off") != std::string::npos) {
      enabled = false;
      detail = "blueutil";
      return true;
    }
  }
  auto out =
      mac_exec("defaults read /Library/Preferences/com.apple.Bluetooth ControllerPowerState");
  auto t = mac_lower(out);
  while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ' || t.back() == ';'))
    t.pop_back();
  if (t == "1") {
    enabled = true;
    detail = "ControllerPowerState";
    return true;
  }
  if (t == "0") {
    enabled = false;
    detail = "ControllerPowerState";
    return true;
  }
  error = "cannot read Bluetooth state (install blueutil: brew install blueutil)";
  return false;
}

bool native_bluetooth_set(bool enabled, std::string& error) {
  if (std::system("command -v blueutil >/dev/null 2>&1") == 0) {
    std::string cmd = std::string("blueutil --power ") + (enabled ? "1" : "0");
    if (std::system((cmd + " >/dev/null 2>&1").c_str()) == 0) {
      bool cur = false;
      std::string detail;
      if (native_bluetooth_status(cur, detail, error) && cur == enabled) return true;
    }
    error = "blueutil failed to change Bluetooth power";
    return false;
  }
  error = "install blueutil to toggle Bluetooth (brew install blueutil)";
  return false;
}

static bool mac_parse_volume(const std::string& txt, int& level, bool& muted) {
  auto low = mac_lower(txt);
  auto pv = low.find("output volume:");
  if (pv == std::string::npos) return false;
  int v = -1;
  try {
    v = std::stoi(low.substr(pv + 14));
  } catch (...) {
    return false;
  }
  level = v;
  auto pm = low.find("output muted:");
  muted = pm != std::string::npos && low.substr(pm).find("true") != std::string::npos;
  return true;
}

bool native_volume_status(int& level, bool& muted, std::string& error) {
  auto out = mac_exec("osascript -e 'get volume settings'");
  if (!mac_parse_volume(out, level, muted)) {
    error = "cannot read volume";
    return false;
  }
  return true;
}

bool native_volume_set(int level, std::string& error) {
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  if (!mac_osascript_ok("set volume output volume " + std::to_string(level))) {
    error = "cannot set volume";
    return false;
  }
  return true;
}

bool native_volume_mute(bool mute, std::string& error) {
  if (!mac_osascript_ok(std::string("set volume output muted ") + (mute ? "true" : "false"))) {
    error = "cannot change mute";
    return false;
  }
  return true;
}

bool native_brightness_status(int& percent, std::string& error) {
#if defined(__APPLE__)
  // IOKit display brightness (built-in panels). External monitors report
  // unsupported and fall through to the error below.
  io_iterator_t it = 0;
  if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IODisplayConnect"),
                                   &it) == KERN_SUCCESS) {
    io_object_t svc = IOIteratorNext(it);
    if (svc) {
      float b = -1;
      if (IODisplayGetFloatParameter(svc, kNilOptions, CFSTR(kIODisplayBrightnessKey), &b) ==
              KERN_SUCCESS &&
          b >= 0) {
        percent = static_cast<int>(b * 100 + 0.5f);
        IOObjectRelease(svc);
        IOObjectRelease(it);
        return true;
      }
      IOObjectRelease(svc);
    }
    IOObjectRelease(it);
  }
#endif
  if (std::system("command -v brightness >/dev/null 2>&1") == 0) {
    auto out = mac_exec("brightness -l");
    // `brightness -l` prints "display 0: brightness 0.75".
    auto low = mac_lower(out);
    auto p = low.find("brightness");
    if (p != std::string::npos) {
      try {
        float f = std::stof(low.substr(p + 10));
        percent = static_cast<int>(f * 100 + 0.5f);
        return true;
      } catch (...) {
      }
    }
  }
  error = "brightness not supported on this display";
  return false;
}

bool native_brightness_set(int percent, std::string& error) {
  if (percent < 0) percent = 0;
  if (percent > 100) percent = 100;
#if defined(__APPLE__)
  io_iterator_t it = 0;
  if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching("IODisplayConnect"),
                                   &it) == KERN_SUCCESS) {
    bool any = false;
    for (io_object_t svc = IOIteratorNext(it); svc; svc = IOIteratorNext(it)) {
      if (IODisplaySetFloatParameter(svc, kNilOptions, CFSTR(kIODisplayBrightnessKey),
                                     static_cast<float>(percent) / 100.0f) == KERN_SUCCESS)
        any = true;
      IOObjectRelease(svc);
    }
    IOObjectRelease(it);
    if (any) return true;
  }
#endif
  if (std::system("command -v brightness >/dev/null 2>&1") == 0) {
    if (std::system(("brightness " + std::to_string(static_cast<float>(percent) / 100.0f) +
                     " >/dev/null 2>&1")
                        .c_str()) == 0)
      return true;
  }
  error = "brightness not supported on this display";
  return false;
}

bool native_open_settings(const std::string& page, std::string& error) {
  auto open_url = [](const std::string& u) {
    return std::system(("open " + mac_shell_quote(u) + " >/dev/null 2>&1").c_str()) == 0;
  };
  auto open_app = [] {
    return std::system("open -a 'System Settings' >/dev/null 2>&1") == 0 ||
           std::system("open -a 'System Preferences' >/dev/null 2>&1") == 0;
  };
  if (page.empty()) {
    if (open_app()) return true;
    error = "cannot open System Settings";
    return false;
  }
  // Ventura+ ids with legacy fallbacks.
  const char* cands[3] = {nullptr, nullptr, nullptr};
  if (page == "wifi") {
    cands[0] = "x-apple.systempreferences:com.apple.wifi-settings";
    cands[1] = "x-apple.systempreferences:com.apple.preference.network";
  } else if (page == "network") {
    cands[0] = "x-apple.systempreferences:com.apple.Network-settings";
    cands[1] = "x-apple.systempreferences:com.apple.preference.network";
  } else if (page == "bluetooth") {
    cands[0] = "x-apple.systempreferences:com.apple.Bluetooth-settings";
    cands[1] = "x-apple.systempreferences:com.apple.preference.bluetooth";
  } else if (page == "sound") {
    cands[0] = "x-apple.systempreferences:com.apple.Sound-settings";
    cands[1] = "x-apple.systempreferences:com.apple.preference.sound";
  } else if (page == "display") {
    cands[0] = "x-apple.systempreferences:com.apple.Display-settings";
    cands[1] = "x-apple.systempreferences:com.apple.preference.displays";
  } else if (page == "battery" || page == "power") {
    cands[0] = "x-apple.systempreferences:com.apple.Battery-settings";
    cands[1] = "x-apple.systempreferences:com.apple.preference.energysaver";
  } else if (page == "apps") {
    cands[0] = "x-apple.systempreferences:com.apple.Applications-settings";
  } else if (page == "privacy") {
    cands[0] = "x-apple.systempreferences:com.apple.settings.PrivacySecurity.extension";
    cands[1] = "x-apple.systempreferences:com.apple.preference.security";
  } else if (page == "update") {
    cands[0] = "x-apple.systempreferences:com.apple.Software-Update-settings";
    cands[1] = "x-apple.systempreferences:com.apple.preferences.softwareupdate";
  } else if (page == "about") {
    cands[0] = "x-apple.systempreferences:com.apple.About-settings";
  } else {
    error = "unknown settings page '" + page + "'";
    return false;
  }
  for (auto* u : cands) {
    if (u && open_url(u)) return true;
  }
  if (open_app()) return true;
  error = "cannot open settings page '" + page + "'";
  return false;
}

#endif
}  // namespace wilfred
