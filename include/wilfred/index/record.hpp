#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace wilfred {

enum class FileKind : std::uint16_t {
  Unknown = 0,
  File,
  Directory,
  Application,
  Executable,
  Document,
  Image,
  Video,
  Audio,
  Archive,
  Source,
  Config,
  Shortcut,
  BrowserData,
};

enum class RecordFlags : std::uint32_t {
  None = 0,
  Hidden = 1u << 0,
  System = 1u << 1,
  Symlink = 1u << 2,
  Executable = 1u << 3,
  Directory = 1u << 4,
  Application = 1u << 5,
  ReadOnly = 1u << 6,
  MountedVolume = 1u << 7,
};

inline RecordFlags operator|(RecordFlags a, RecordFlags b) {
  return static_cast<RecordFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}
inline RecordFlags operator&(RecordFlags a, RecordFlags b) {
  return static_cast<RecordFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}
inline bool has_flag(RecordFlags a, RecordFlags b) {
  return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b)) != 0;
}

struct IndexRecord {
  std::uint32_t id{0};
  std::uint32_t name_id{0};
  std::uint32_t path_id{0};
  std::uint32_t parent_id{0};
  std::uint32_t ext_id{0};
  std::uint64_t size{0};
  std::int64_t ctime{0};
  std::int64_t mtime{0};
  std::int64_t atime{0};
  RecordFlags flags{RecordFlags::None};
  std::uint16_t volume_id{0};
  FileKind kind{FileKind::Unknown};
  std::uint32_t mode{0};
};

std::string_view kind_name(FileKind k);
FileKind kind_from_name(std::string_view n);

}  // namespace wilfred
