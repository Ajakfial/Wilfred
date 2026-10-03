#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

#include <cstdlib>

namespace wilfred {
#if defined(WILFRED_IOS)

std::vector<VolumeInfo> native_list_volumes() {
  std::vector<VolumeInfo> out;
  // Only the app sandbox is visible. $HOME inside the sandbox is the app
  // container, which the ObjC++ bridge also passes as files_dir.
  if (const char* home = std::getenv("HOME"); home && *home) {
    VolumeInfo app;
    app.path = home;
    app.name = "App files";
    app.fs_type = "apfs";
    app.ready = true;
    out.push_back(app);
  }
  return out;
}

#endif
}  // namespace wilfred
