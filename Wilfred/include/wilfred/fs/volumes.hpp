#pragma once

#include <string>
#include <vector>

namespace wilfred {

struct VolumeInfo {
  std::string path;
  std::string name;
  std::string fs_type;
  bool removable{false};
  bool network{false};
  bool ready{true};
};

std::vector<VolumeInfo> list_volumes();

}  // namespace wilfred
