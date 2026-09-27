#include "wilfred/platform/native.hpp"

#include "wilfred/core/mmap.hpp"

#include <fstream>
#include <sstream>

namespace wilfred {
#if !defined(_WIN32) && !defined(__APPLE__)

std::vector<VolumeInfo> native_list_volumes() {
  std::vector<VolumeInfo> out;
  std::ifstream in("/proc/mounts");
  std::string line;
  while (std::getline(in, line)) {
    std::istringstream is(line);
    std::string dev, mp, type;
    is >> dev >> mp >> type;
    if (mp.empty()) continue;
    if (mp.rfind("/proc", 0) == 0 || mp.rfind("/sys", 0) == 0 || mp.rfind("/dev", 0) == 0 ||
        mp == "/") {
      if (mp == "/") {
        VolumeInfo v;
        v.path = "/";
        v.name = "root";
        v.fs_type = type;
        out.push_back(v);
      }
      continue;
    }
    VolumeInfo v;
    v.path = mp;
    v.name = mp;
    v.fs_type = type;
    v.network = type.find("nfs") != std::string::npos || type.find("cifs") != std::string::npos ||
                type.find("smb") != std::string::npos;
    v.removable = mp.rfind("/media/", 0) == 0 || mp.rfind("/run/media/", 0) == 0 ||
                  mp.rfind("/mnt/", 0) == 0;
    out.push_back(std::move(v));
  }
  return out;
}

#endif
}  // namespace wilfred
