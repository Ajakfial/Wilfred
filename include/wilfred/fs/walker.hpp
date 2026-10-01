#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/fs/classify.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
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

// Fast path: platform-accelerated enumeration (Windows USN journal/MFT scan)
// with automatic fallback to walk_tree. Same contract as walk_tree.
void fast_enumerate_tree(const std::string& root, const Config& cfg, WalkFn on_entry,
                         std::atomic<bool>* cancel, WalkStats* stats);

#ifdef _WIN32
struct UsnNode {
  std::uint64_t parent{0};
  std::wstring name;
  bool is_dir{false};
};

// Pure parent-chain resolver (unit-testable): maps every node FRN to its
// volume-absolute path. Nodes whose chain is broken resolve under the volume
// prefix; cycles are cut.
std::unordered_map<std::uint64_t, std::string> usn_resolve_paths(
    const std::unordered_map<std::uint64_t, UsnNode>& nodes, const std::string& volume_prefix);

// Full MFT enumeration for root; false when unavailable (non-NTFS,
// no elevation, ...) so the caller falls back to walk_tree.
bool win_usn_enumerate_tree(const std::string& root, const Config& cfg, WalkFn on_entry,
                            std::atomic<bool>* cancel, WalkStats* stats);
#endif

}  // namespace wilfred
