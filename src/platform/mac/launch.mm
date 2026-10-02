#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#include <cstdlib>
#include <string>

#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreServices/CoreServices.h>

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

#endif
}  // namespace wilfred
