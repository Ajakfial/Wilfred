#include "wilfred/platform/native.hpp"

#ifdef __APPLE__
#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#include <cstdio>
#include <string>
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

bool native_focus_window(std::uint64_t window_id) {
  if (!window_id) return false;
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
      if (!wid || wid.unsignedLongLongValue != window_id) continue;
      NSNumber* owner_pid = info[(id)kCGWindowOwnerPID];
      pid = owner_pid ? static_cast<pid_t>(owner_pid.intValue) : 0;
      want_title = info[(id)kCGWindowName];
      break;
    }
    CFRelease(list);
    if (!pid) return false;

    NSRunningApplication* app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    if (app) {
      [app activateWithOptions:NSApplicationActivateAllWindows];
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
        NSString* title = (__bridge NSString*)title_ref;
        if (want_title && title && [title isEqualToString:want_title]) {
          AXUIElementPerformAction(win, kAXRaiseAction);
          AXUIElementSetAttributeValue(win, kAXMainAttribute, kCFBooleanTrue);
          AXUIElementSetAttributeValue(win, kAXFocusedAttribute, kCFBooleanTrue);
          raised = true;
        }
        CFRelease(title_ref);
        if (raised) break;
      }
    }
    if (wins_ref) CFRelease(wins_ref);
    CFRelease(app_el);
    return raised || app != nil;
  }
}

// Resolve the AX window element for a CGWindowNumber, mirroring the lookup
// in native_focus_window. Title matching breaks on duplicate titles; callers
// operate on the first match, same as focus.
static AXUIElementRef ax_window_for_id(std::uint64_t window_id, pid_t* pid_out) {
  if (!window_id) return nullptr;
  CFArrayRef list = CGWindowListCopyWindowInfo(
      kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
  if (!list) return nullptr;
  pid_t pid = 0;
  NSString* want_title = nil;
  CFIndex n = CFArrayGetCount(list);
  for (CFIndex i = 0; i < n; ++i) {
    NSDictionary* info = (__bridge NSDictionary*)CFArrayGetValueAtIndex(list, i);
    NSNumber* wid = info[(id)kCGWindowNumber];
    if (!wid || wid.unsignedLongLongValue != window_id) continue;
    NSNumber* owner_pid = info[(id)kCGWindowOwnerPID];
    pid = owner_pid ? static_cast<pid_t>(owner_pid.intValue) : 0;
    want_title = info[(id)kCGWindowName];
    break;
  }
  CFRelease(list);
  if (!pid) return nullptr;
  if (pid_out) *pid_out = pid;
  AXUIElementRef app_el = AXUIElementCreateApplication(pid);
  if (!app_el) return nullptr;
  AXUIElementRef found = nullptr;
  CFTypeRef wins_ref = nullptr;
  if (AXUIElementCopyAttributeValue(app_el, kAXWindowsAttribute, &wins_ref) == kAXErrorSuccess &&
      wins_ref && CFGetTypeID(wins_ref) == CFArrayGetTypeID()) {
    CFArrayRef wins = (CFArrayRef)wins_ref;
    CFIndex wn = CFArrayGetCount(wins);
    for (CFIndex i = 0; i < wn; ++i) {
      AXUIElementRef win = (AXUIElementRef)CFArrayGetValueAtIndex(wins, i);
      CFTypeRef title_ref = nullptr;
      bool match = false;
      if (AXUIElementCopyAttributeValue(win, kAXTitleAttribute, &title_ref) == kAXErrorSuccess) {
        NSString* title = (__bridge NSString*)title_ref;
        match = !want_title || !title || [title isEqualToString:want_title];
      } else {
        // Untitled windows (e.g. blank editors) match when the target is untitled.
        match = !want_title;
      }
      if (title_ref) CFRelease(title_ref);
      if (match) {
        found = win;
        CFRetain(found);
        break;
      }
    }
  }
  if (wins_ref) CFRelease(wins_ref);
  CFRelease(app_el);
  return found;
}

static bool ax_set_bool(AXUIElementRef win, CFStringRef attr, bool value) {
  return AXUIElementSetAttributeValue(win, attr, value ? kCFBooleanTrue : kCFBooleanFalse) ==
         kAXErrorSuccess;
}

static std::string ax_error(const char* what, AXError err) {
  char buf[96];
  std::snprintf(buf, sizeof(buf), "%s (AX error %d; needs Accessibility permission)", what,
                static_cast<int>(err));
  return buf;
}

static CGRect main_display_bounds() { return CGDisplayBounds(kCGDirectMainDisplay); }

// Set an AX window frame. Returns true only if both position and size stuck.
static bool ax_set_frame(AXUIElementRef win, CGFloat x, CGFloat y, CGFloat w, CGFloat h,
                         AXError& err_out) {
  CGPoint pos = CGPointMake(x, y);
  CGSize size = CGSizeMake(w, h);
  AXValueRef pos_v = AXValueCreate(static_cast<AXValueType>(kAXValueCGPointType), &pos);
  AXValueRef size_v = AXValueCreate(static_cast<AXValueType>(kAXValueCGSizeType), &size);
  AXError pe = pos_v ? AXUIElementSetAttributeValue(win, kAXPositionAttribute, pos_v)
                     : kAXErrorFailure;
  AXError se = size_v ? AXUIElementSetAttributeValue(win, kAXSizeAttribute, size_v)
                      : kAXErrorFailure;
  if (pos_v) CFRelease(pos_v);
  if (size_v) CFRelease(size_v);
  err_out = pe != kAXErrorSuccess ? pe : se;
  return pe == kAXErrorSuccess && se == kAXErrorSuccess;
}

bool native_window_action(std::uint64_t id, NativeWindowOp op, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  @autoreleasepool {
    AXUIElementRef win = ax_window_for_id(id, nullptr);
    if (!win) {
      error = "window no longer exists";
      return false;
    }
    bool ok = false;
    if (op == NativeWindowOp::Minimize) {
      ok = ax_set_bool(win, kAXMinimizedAttribute, true);
      if (!ok) error = ax_error("could not minimize window", kAXErrorFailure);
    } else if (op == NativeWindowOp::Restore) {
      ok = ax_set_bool(win, kAXMinimizedAttribute, false);
      if (ok) {
        AXUIElementPerformAction(win, kAXRaiseAction);
        ax_set_bool(win, kAXFocusedAttribute, true);
      } else {
        error = ax_error("could not restore window", kAXErrorFailure);
      }
    } else if (op == NativeWindowOp::Maximize) {
      // No settable zoom attribute exists in the SDK; maximize by filling
      // the main display, matching what layouts record and restore.
      ax_set_bool(win, kAXMinimizedAttribute, false);
      CGRect bounds = main_display_bounds();
      AXError errc = kAXErrorSuccess;
      ok = ax_set_frame(win, bounds.origin.x, bounds.origin.y, bounds.size.width,
                        bounds.size.height, errc);
      if (!ok) error = ax_error("could not maximize window", errc);
    } else if (op == NativeWindowOp::Close) {
      CFTypeRef btn_ref = nullptr;
      if (AXUIElementCopyAttributeValue(win, kAXCloseButtonAttribute, &btn_ref) ==
              kAXErrorSuccess &&
          btn_ref) {
        ok = AXUIElementPerformAction((AXUIElementRef)btn_ref, kAXPressAction) == kAXErrorSuccess;
        CFRelease(btn_ref);
      }
      if (!ok) error = ax_error("could not close window", kAXErrorFailure);
    } else {
      // SnapLeft/SnapRight: half of the main display via position+size.
      ax_set_bool(win, kAXMinimizedAttribute, false);
      CGRect bounds = main_display_bounds();
      CGFloat half = bounds.size.width / 2;
      CGFloat x = op == NativeWindowOp::SnapLeft ? bounds.origin.x : bounds.origin.x + half;
      AXError errc = kAXErrorSuccess;
      ok = ax_set_frame(win, x, bounds.origin.y, half, bounds.size.height, errc);
      if (!ok) error = ax_error("could not snap window", errc);
    }
    CFRelease(win);
    return ok;
  }
}

bool native_window_rect(std::uint64_t id, NativeWindowRect& rect, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  @autoreleasepool {
    AXUIElementRef win = ax_window_for_id(id, nullptr);
    if (!win) {
      error = "window no longer exists";
      return false;
    }
    CGPoint pos = CGPointZero;
    CGSize size = CGSizeZero;
    CFTypeRef pos_ref = nullptr;
    CFTypeRef size_ref = nullptr;
    bool ok = AXUIElementCopyAttributeValue(win, kAXPositionAttribute, &pos_ref) ==
                  kAXErrorSuccess &&
              AXUIElementCopyAttributeValue(win, kAXSizeAttribute, &size_ref) == kAXErrorSuccess;
    if (ok && pos_ref && size_ref) {
      AXValueGetValue((AXValueRef)pos_ref, static_cast<AXValueType>(kAXValueCGPointType), &pos);
      AXValueGetValue((AXValueRef)size_ref, static_cast<AXValueType>(kAXValueCGSizeType), &size);
      rect.x = static_cast<int>(pos.x);
      rect.y = static_cast<int>(pos.y);
      rect.w = static_cast<int>(size.width);
      rect.h = static_cast<int>(size.height);
      // No zoomed/fullscreen attribute exists in the SDK; treat a window
      // filling the main display as maximized.
      CGRect bounds = main_display_bounds();
      rect.maximized = rect.x == static_cast<int>(bounds.origin.x) &&
                       rect.y == static_cast<int>(bounds.origin.y) &&
                       rect.w == static_cast<int>(bounds.size.width) &&
                       rect.h == static_cast<int>(bounds.size.height);
    } else {
      error = ax_error("could not read window rect", kAXErrorFailure);
      ok = false;
    }
    if (pos_ref) CFRelease(pos_ref);
    if (size_ref) CFRelease(size_ref);
    CFRelease(win);
    return ok;
  }
}

bool native_window_move(std::uint64_t id, int x, int y, int w, int h, std::string& error) {
  if (!id) {
    error = "invalid window";
    return false;
  }
  @autoreleasepool {
    AXUIElementRef win = ax_window_for_id(id, nullptr);
    if (!win) {
      error = "window no longer exists";
      return false;
    }
    ax_set_bool(win, kAXMinimizedAttribute, false);
    CGPoint pos = CGPointMake(x, y);
    AXValueRef pos_v = AXValueCreate(static_cast<AXValueType>(kAXValueCGPointType), &pos);
    AXError pe = pos_v ? AXUIElementSetAttributeValue(win, kAXPositionAttribute, pos_v)
                       : kAXErrorFailure;
    if (pos_v) CFRelease(pos_v);
    bool ok = pe == kAXErrorSuccess;
    if (ok && w > 0 && h > 0) {
      CGSize size = CGSizeMake(w, h);
      AXValueRef size_v = AXValueCreate(static_cast<AXValueType>(kAXValueCGSizeType), &size);
      AXError se = size_v ? AXUIElementSetAttributeValue(win, kAXSizeAttribute, size_v)
                          : kAXErrorFailure;
      if (size_v) CFRelease(size_v);
      ok = se == kAXErrorSuccess;
    }
    CFRelease(win);
    if (!ok) {
      error = ax_error("could not move window", kAXErrorFailure);
      return false;
    }
    return true;
  }
}

#endif
}  // namespace wilfred
