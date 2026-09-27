#include "wilfred/platform/native.hpp"

#include "wilfred/core/utf8.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {
#ifdef _WIN32

std::vector<VolumeInfo> native_list_volumes() {
  std::vector<VolumeInfo> out;
  DWORD mask = GetLogicalDrives();
  for (int i = 0; i < 26; ++i) {
    if ((mask & (1u << i)) == 0) continue;
    wchar_t root[] = {static_cast<wchar_t>(L'A' + i), L':', L'\\', 0};
    UINT type = GetDriveTypeW(root);
    if (type == DRIVE_NO_ROOT_DIR) continue;
    wchar_t name[MAX_PATH]{}, fs[64]{};
    GetVolumeInformationW(root, name, MAX_PATH, nullptr, nullptr, nullptr, fs, 64);
    VolumeInfo v;
    v.path = wide_to_utf8(root);
    v.name = wide_to_utf8(name);
    v.fs_type = wide_to_utf8(fs);
    v.removable = type == DRIVE_REMOVABLE || type == DRIVE_CDROM;
    v.network = type == DRIVE_REMOTE;
    v.ready = type != DRIVE_NO_ROOT_DIR;
    out.push_back(std::move(v));
  }
  return out;
}

#else
std::vector<VolumeInfo> native_list_volumes();
#endif
}  // namespace wilfred
