#include "wilfred/platform/native.hpp"

#if defined(__APPLE__) && !defined(IOS)
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <CoreGraphics/CoreGraphics.h>

#include <dispatch/dispatch.h>
#include <string>
#include <vector>

namespace wilfred {

namespace {

bool sample_point(int x, int y, NativeColor& out, std::string& error) {
  // Find the display containing the point.
  CGDisplayCount count = 0;
  CGGetActiveDisplayList(0, nullptr, &count);
  std::vector<CGDirectDisplayID> displays(count);
  if (count > 0) CGGetActiveDisplayList(count, displays.data(), &count);
  CGDirectDisplayID target = CGMainDisplayID();
  for (CGDisplayCount i = 0; i < count; ++i) {
    if (CGRectContainsPoint(CGDisplayBounds(displays[i]), CGPointMake(x, y))) {
      target = displays[i];
      break;
    }
  }

  // Map CGDirectDisplayID to SCDisplay.
  dispatch_semaphore_t sem1 = dispatch_semaphore_create(0);
  __block SCShareableContent* content = nil;
  __block NSError* cErr = nil;
  [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent* sc, NSError* err) {
    content = sc;
    cErr = err;
    dispatch_semaphore_signal(sem1);
  }];
  dispatch_semaphore_wait(sem1, DISPATCH_TIME_FOREVER);
  if (!content || cErr) {
    error = "Screen capture requires Screen Recording permission";
    return false;
  }
  SCDisplay* scDisplay = nil;
  for (SCDisplay* d in content.displays) {
    if (d.displayID == target) { scDisplay = d; break; }
  }
  if (!scDisplay) {
    error = "Could not find the display for that point";
    return false;
  }

  SCContentFilter* filter = [[SCContentFilter alloc] initWithDisplay:scDisplay excludingWindows:@[]];
  SCStreamConfiguration* cfg = [[SCStreamConfiguration alloc] init];
  CGRect bounds = CGDisplayBounds(target);
  cfg.sourceRect = CGRectMake(x - bounds.origin.x, y - bounds.origin.y, 1, 1);
  cfg.width = 1;
  cfg.height = 1;

  dispatch_semaphore_t sem2 = dispatch_semaphore_create(0);
  __block CGImageRef image = nullptr;
  __block NSError* iErr = nullptr;
  [SCScreenshotManager captureImageWithFilter:filter
                               configuration:cfg
                           completionHandler:^(CGImageRef img, NSError* err) {
    if (img) image = CGImageCreateCopy(img);
    iErr = err;
    dispatch_semaphore_signal(sem2);
  }];
  dispatch_semaphore_wait(sem2, DISPATCH_TIME_FOREVER);
  if (!image || iErr) {
    error = "Could not sample the screen";
    return false;
  }

  // Read the single pixel.
  CGDataProviderRef prov = CGImageGetDataProvider(image);
  CFDataRef data = CGDataProviderCopyData(prov);
  if (!data) {
    CGImageRelease(image);
    error = "Could not read screen pixels";
    return false;
  }
  const unsigned char* bytes = CFDataGetBytePtr(data);
  // ScreenCaptureKit returns BGRA (little-endian) typically.
  out.b = bytes[0];
  out.g = bytes[1];
  out.r = bytes[2];
  CFRelease(data);
  CGImageRelease(image);
  return true;
}

}  // namespace

bool native_color_at_point(int x, int y, NativeColor& out, std::string& error) {
  error.clear();
  return sample_point(x, y, out, error);
}

bool native_color_at_cursor(NativeColor& out, std::string& error) {
  error.clear();
  CGEventRef ev = CGEventCreate(nullptr);
  if (!ev) { error = "Could not read cursor position"; return false; }
  CGPoint p = CGEventGetLocation(ev);
  CFRelease(ev);
  return sample_point((int)p.x, (int)p.y, out, error);
}

}  // namespace wilfred
#else
namespace wilfred {
bool native_color_at_point(int, int, NativeColor&, std::string&);
bool native_color_at_cursor(NativeColor&, std::string&);
}  // namespace wilfred
#endif