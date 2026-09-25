#include "wilfred/platform/native.hpp"

#import <Foundation/Foundation.h>

namespace wilfred {
#ifdef __APPLE__

std::vector<VolumeInfo> native_list_volumes() {
  std::vector<VolumeInfo> out;
  @autoreleasepool {
    NSArray* urls = [[NSFileManager defaultManager]
        mountedVolumeURLsIncludingResourceValuesForKeys:@[ NSURLVolumeNameKey, NSURLVolumeIsRemovableKey ]
                                                options:0];
    for (NSURL* url in urls) {
      VolumeInfo v;
      v.path = std::string([[url path] UTF8String] ? [[url path] UTF8String] : "");
      NSString* name = nil;
      [url getResourceValue:&name forKey:NSURLVolumeNameKey error:nil];
      if (name) v.name = [name UTF8String];
      NSNumber* rem = nil;
      [url getResourceValue:&rem forKey:NSURLVolumeIsRemovableKey error:nil];
      v.removable = rem && [rem boolValue];
      NSNumber* local = nil;
      [url getResourceValue:&local forKey:NSURLVolumeIsLocalKey error:nil];
      v.network = local && ![local boolValue];
      out.push_back(std::move(v));
    }
  }
  return out;
}

#endif
}  // namespace wilfred
