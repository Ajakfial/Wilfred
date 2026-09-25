#include "wilfred/fs/volumes.hpp"

#include "wilfred/platform/native.hpp"

namespace wilfred {

std::vector<VolumeInfo> list_volumes() { return native_list_volumes(); }

}  // namespace wilfred
