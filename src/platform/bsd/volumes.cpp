#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"

#include <string>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ucred.h>

namespace wilfred {
#if defined(WILFRED_BSD)

// getmntinfo(2) snapshot instead of Linux's /proc/mounts (absent on BSD
// unless procfs/linprocfs is mounted). Pseudo filesystems are skipped the
// same way the Linux backend skips /proc, /sys and /dev; "/" is always
// reported as root so callers have at least one volume.
std::vector<VolumeInfo> native_list_volumes() {
  std::vector<VolumeInfo> out;
  struct statfs* mnt = nullptr;
  int n = getmntinfo(&mnt, MNT_NOWAIT);
  if (n <= 0 || !mnt) {
    VolumeInfo v;
    v.path = "/";
    v.name = "root";
    out.push_back(v);
    return out;
  }
  auto skip_pseudo = [](const std::string& type) {
    return type == "devfs" || type == "procfs" || type == "linprocfs" ||
           type == "fdescfs" || type == "tmpfs";
  };
  for (int i = 0; i < n; ++i) {
    std::string mp = mnt[i].f_mntonname;
    std::string type = mnt[i].f_fstypename;
    if (mp.empty()) continue;
    if (mp == "/") {
      VolumeInfo v;
      v.path = "/";
      v.name = "root";
      v.fs_type = type;
      out.push_back(std::move(v));
      continue;
    }
    if (skip_pseudo(type)) continue;
    VolumeInfo v;
    v.path = mp;
    v.name = mp;
    v.fs_type = type;
    v.network = type.find("nfs") != std::string::npos || type.find("smb") != std::string::npos;
    v.removable = mp.rfind("/media/", 0) == 0 || mp.rfind("/mnt/", 0) == 0;
    out.push_back(std::move(v));
  }
  return out;
}

#endif
}  // namespace wilfred
