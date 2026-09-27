#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/fs/classify.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace wilfred {

struct WalkEntry {
  std::string path;
  FileStat st;
};

using WalkFn = std::function<void(const WalkEntry&)>;

struct WalkStats {
  std::uint64_t visited{0};
  std::uint64_t skipped{0};
  std::uint64_t errors{0};
};

void walk_tree(const std::string& root, const Config& cfg, WalkFn on_entry,
               std::atomic<bool>* cancel, WalkStats* stats);

}  // namespace wilfred
