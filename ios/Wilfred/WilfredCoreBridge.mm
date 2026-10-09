// Wilfred iOS core bridge (ObjC++): thin wrapper around wilfred::MobileCore,
// the same shared core the Android JNI layer uses. The C++ core is already
// thread-safe (internal mutex); the bridge adds no extra locking.

#import "WilfredCoreBridge.h"

#include <mutex>
#include <string>

#include "wilfred/platform/android_bridge.hpp"

namespace {

std::string NSStringToUTF8(NSString* s) {
  if (s == nil) return {};
  return std::string([s UTF8String]);
}

NSString* UTF8ToNSString(const std::string& s) {
  return [NSString stringWithUTF8String:s.c_str()];
}

}  // namespace

@implementation WilfredCoreBridge {
  wilfred::MobileCore _core;
  std::mutex _mu;
}

- (BOOL)bootWithFilesDir:(NSString*)dir error:(NSString**)error {
  std::lock_guard<std::mutex> lock(_mu);
  std::string err;
  BOOL ok = _core.boot(NSStringToUTF8(dir), err) ? YES : NO;
  if (!ok && error) *error = UTF8ToNSString(err);
  return ok;
}

- (NSString*)searchJSON:(NSString*)query limit:(NSInteger)limit {
  std::lock_guard<std::mutex> lock(_mu);
  return UTF8ToNSString(_core.search_json(NSStringToUTF8(query), (int)limit));
}

- (NSString*)assistJSON:(NSString*)query {
  std::lock_guard<std::mutex> lock(_mu);
  return UTF8ToNSString(_core.assist_json(NSStringToUTF8(query)));
}

- (NSString*)actionsJSON:(NSInteger)index {
  std::lock_guard<std::mutex> lock(_mu);
  return UTF8ToNSString(_core.actions_json((std::size_t)index));
}

- (BOOL)executeAction:(NSInteger)index actionId:(NSString*)actionId error:(NSString**)error {
  std::lock_guard<std::mutex> lock(_mu);
  std::string err;
  BOOL ok = _core.execute_action((std::size_t)index, NSStringToUTF8(actionId), err) ? YES : NO;
  if (!ok && error) *error = UTF8ToNSString(err);
  return ok;
}

- (NSString*)statusJSON {
  std::lock_guard<std::mutex> lock(_mu);
  return UTF8ToNSString(_core.status_json());
}

- (BOOL)indexNow:(NSString**)error {
  std::lock_guard<std::mutex> lock(_mu);
  std::string err;
  BOOL ok = _core.index_now(err) ? YES : NO;
  if (!ok && error) *error = UTF8ToNSString(err);
  return ok;
}

- (BOOL)recordChoice:(NSString*)query key:(NSString*)key {
  std::lock_guard<std::mutex> lock(_mu);
  return _core.record_choice(NSStringToUTF8(query), NSStringToUTF8(key)) ? YES : NO;
}

- (void)setClipboard:(NSString*)text {
  std::lock_guard<std::mutex> lock(_mu);
  _core.set_clipboard(NSStringToUTF8(text));
}

- (NSString*)previewJSON:(NSString*)path {
  std::lock_guard<std::mutex> lock(_mu);
  return UTF8ToNSString(_core.preview_json(NSStringToUTF8(path)));
}

@end
