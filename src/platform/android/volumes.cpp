#include "wilfred/platform/native.hpp"

#include <cstdlib>

namespace wilfred {
#if defined(__ANDROID__)

std::vector<VolumeInfo> native_list_volumes() {
  std::vector<VolumeInfo> out;
  // Public shared storage plus the app-private files dir (via $HOME, which
  // the JNI layer points at Context.getFilesDir()).
  VolumeInfo shared;
  shared.path = "/sdcard";
  shared.name = "Shared storage";
  shared.fs_type = "sdcardfs";
  shared.ready = true;
  out.push_back(shared);

  if (const char* home = std::getenv("HOME"); home && *home) {
    VolumeInfo app;
    app.path = home;
    app.name = "App files";
    app.fs_type = "ext4";
    app.ready = true;
    out.push_back(app);
  }
  return out;
}

#endif
}  // namespace wilfred
