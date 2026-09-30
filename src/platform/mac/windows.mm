#include "wilfred/platform/native.hpp"

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#endif

namespace wilfred {
#ifdef __APPLE__

std::vector<NativeWindowInfo> native_list_windows() {
  std::vector<NativeWindowInfo> out;
  @autoreleasepool {
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (!list) return out;
    pid_t self = getpid();
    CFIndex n = CFArrayGetCount(list);
    for (CFIndex i = 0; i < n; ++i) {
      NSDictionary* info = (__bridge NSDictionary*)CFArrayGetValueAtIndex(list, i);
      NSNumber* layer = info[(id)kCGWindowLayer];
      if (layer && layer.intValue != 0) continue;
      NSString* name = info[(id)kCGWindowName];
      NSNumber* wid = info[(id)kCGWindowNumber];
      NSNumber* pid = info[(id)kCGWindowOwnerPID];
      NSString* owner = info[(id)kCGWindowOwnerName];
      if (pid && pid.intValue == self) continue;
      NativeWindowInfo w;
      w.id = wid ? static_cast<std::uint64_t>(wid.unsignedLongLongValue) : 0;
      if (!w.id) continue;
      w.owner = owner && owner.UTF8String ? owner.UTF8String : "";
      if (name && name.length > 0 && name.UTF8String)
        w.title = name.UTF8String;
      else if (!w.owner.empty())
        w.title = w.owner;
      else
        continue;
      out.push_back(std::move(w));
    }
    CFRelease(list);
  }
  return out;
}

bool native_focus_window(std::uint64_t id) {
  if (!id) return false;
  @autoreleasepool {
    CFArrayRef list = CGWindowListCopyWindowInfo(
        kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (!list) return false;
    pid_t pid = 0;
    NSString* want_title = nil;
    CFIndex n = CFArrayGetCount(list);
    for (CFIndex i = 0; i < n; ++i) {
      NSDictionary* info = (__bridge NSDictionary*)CFArrayGetValueAtIndex(list, i);
      NSNumber* wid = info[(id)kCGWindowNumber];
      if (!wid || wid.unsignedLongLongValue != id) continue;
      NSNumber* owner_pid = info[(id)kCGWindowOwnerPID];
      pid = owner_pid ? static_cast<pid_t>(owner_pid.intValue) : 0;
      want_title = info[(id)kCGWindowName];
      break;
    }
    CFRelease(list);
    if (!pid) return false;

    NSRunningApplication* app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if (app) {
      [app activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    }

    AXUIElementRef app_el = AXUIElementCreateApplication(pid);
    if (!app_el) return app != nil;
    CFTypeRef wins_ref = nullptr;
    AXError err = AXUIElementCopyAttributeValue(app_el, kAXWindowsAttribute, &wins_ref);
    bool raised = false;
    if (err == kAXErrorSuccess && wins_ref && CFGetTypeID(wins_ref) == CFArrayGetTypeID()) {
      CFArrayRef wins = (CFArrayRef)wins_ref;
      CFIndex wn = CFArrayGetCount(wins);
      for (CFIndex i = 0; i < wn; ++i) {
        AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(wins, i);
        CFTypeRef title_ref = nullptr;
        if (AXUIElementCopyAttributeValue(win, kAXTitleAttribute, &title_ref) != kAXErrorSuccess)
          continue;
        NSString* title = (__bridge_transfer NSString*)title_ref;
        if (want_title && title && [title isEqualToString:want_title]) {
          AXUIElementPerformAction(win, kAXRaiseAction);
          AXUIElementSetAttributeValue(win, kAXMainAttribute, kCFBooleanTrue);
          AXUIElementSetAttributeValue(win, kAXFocusedAttribute, kCFBooleanTrue);
          raised = true;
          break;
        }
      }
    }
    if (wins_ref) CFRelease(wins_ref);
    CFRelease(app_el);
    return raised || app != nil;
  }
}

#endif
}  // namespace wilfred
