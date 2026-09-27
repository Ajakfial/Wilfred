#pragma once

#include "wilfred/index/record.hpp"

#include <string>

namespace wilfred {

// Small fitted icon as a data URL, or empty when none is available.
std::string file_icon_data_url(const std::string& path, FileKind kind);

}  // namespace wilfred
