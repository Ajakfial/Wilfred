#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

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

#endif
}  // namespace wilfred
