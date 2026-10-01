#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#include <cstdlib>
#include <string>

#import <AppKit/AppKit.h>
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
    CFArrayRef apps = LSCopyApplicationURLsForURL((__bridge CFURLRef)url, kLSRolesAll, NULL);
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

#endif
}  // namespace wilfred
