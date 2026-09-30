#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#include <cstdlib>

#import <AppKit/AppKit.h>

namespace wilfred {
#ifdef __APPLE__

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

#endif
}  // namespace wilfred
