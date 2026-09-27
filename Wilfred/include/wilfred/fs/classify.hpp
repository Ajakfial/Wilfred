#pragma once

#include "wilfred/index/record.hpp"

#include <cstdint>
#include <string>
#include <string_view>

namespace wilfred {

FileKind classify_extension(std::string_view ext);
FileKind classify_path(std::string_view path, bool is_dir, bool is_exec);
bool looks_hidden(std::string_view name, std::string_view path);
bool looks_system_name(std::string_view name);

struct FileStat {
  bool exists{false};
  bool is_dir{false};
  bool is_file{false};
  bool is_symlink{false};
  bool is_hidden{false};
  bool is_system{false};
  bool is_exec{false};
  bool readable{true};
  std::uint64_t size{0};
  std::int64_t ctime{0};
  std::int64_t mtime{0};
  std::int64_t atime{0};
  std::uint32_t mode{0};
};

FileStat stat_path(const std::string& path);

}  // namespace wilfred
