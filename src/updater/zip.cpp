// Minimal ZIP archive extractor.
// Supports stored (method 0) and deflate (method 8) compression.

#include "wilfred/updater/updater.hpp"
#include "wilfred/updater/inflate.hpp"

#include "wilfred/core/log.hpp"

#include <cstring>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace wilfred {

namespace {

// NOTE: ZIP on-disk headers are packed with no padding, which C structs
// don't reproduce (e.g. a u32 at file offset 14 or 38 forces pad bytes, so
// sizeof() != on-disk size). All header fields are read explicitly with
// rd16/rd32 below; do not memcpy these structs from file bytes.
struct ZipEndRecord {
  std::uint32_t signature;    // 0x06054b50
  std::uint16_t disk_num;
  std::uint16_t disk_start;
  std::uint16_t entries_here;
  std::uint16_t total_entries;
  std::uint32_t central_size;
  std::uint32_t central_offset;
  std::uint16_t comment_len;
};

std::uint16_t rd16(const std::uint8_t* p) {
  return static_cast<std::uint16_t>(p[0] | (static_cast<std::uint16_t>(p[1]) << 8));
}

std::uint32_t rd32(const std::uint8_t* p) {
  return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& data) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  f.seekg(0, std::ios::end);
  std::size_t sz = static_cast<std::size_t>(f.tellg());
  f.seekg(0, std::ios::beg);
  data.resize(sz);
  if (sz > 0) f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(sz));
  return f.good() || f.eof();
}

bool write_file(const std::string& path, const std::vector<std::uint8_t>& data) {
  std::string dir = path;
  auto slash = dir.find_last_of("/\\");
  if (slash != std::string::npos) {
    dir = dir.substr(0, slash);
    // Create directories recursively
    std::string cur;
    for (char c : dir) {
      cur.push_back(c);
      if (c == '/' || c == '\\') {
        if (cur != "/" && cur != "\\") {
#ifdef _WIN32
          CreateDirectoryA(cur.c_str(), nullptr);
#else
          mkdir(cur.c_str(), 0755);
#endif
        }
      }
    }
#ifdef _WIN32
    CreateDirectoryA(dir.c_str(), nullptr);
#else
    mkdir(dir.c_str(), 0755);
#endif
  }

  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return false;
  if (!data.empty()) {
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
  }
  return f.good();
}

}  // namespace

bool extract_zip(const std::string& archive_path, const std::string& dest_dir,
                 std::string* error) {
  std::vector<std::uint8_t> data;
  if (!read_file(archive_path, data)) {
    if (error) *error = "cannot read archive: " + archive_path;
    return false;
  }

  if (data.size() < 22) {
    if (error) *error = "archive too small";
    return false;
  }

  // Find End of Central Directory record (search from end)
  std::size_t eocd_pos = std::string::npos;
  for (std::size_t i = data.size() - 22; i + 22 <= data.size(); --i) {
    if (data[i] == 'P' && data[i + 1] == 'K' && data[i + 2] == 5 && data[i + 3] == 6) {
      eocd_pos = i;
      break;
    }
    if (i == 0) break;
  }

  if (eocd_pos == std::string::npos) {
    if (error) *error = "invalid ZIP: end of central directory not found";
    return false;
  }

  ZipEndRecord eocd;
  {
    const std::uint8_t* p = data.data() + eocd_pos;
    eocd.signature = rd32(p);
    eocd.disk_num = rd16(p + 4);
    eocd.disk_start = rd16(p + 6);
    eocd.entries_here = rd16(p + 8);
    eocd.total_entries = rd16(p + 10);
    eocd.central_size = rd32(p + 12);
    eocd.central_offset = rd32(p + 16);
    eocd.comment_len = rd16(p + 20);
  }
  // Byte-swap if needed (ZIP is little-endian, we assume LE host)
  // On LE hosts this is a no-op

  std::uint16_t total = eocd.total_entries;
  std::uint32_t offset = eocd.central_offset;

  for (std::uint16_t i = 0; i < total; ++i) {
    if (offset + 46 > data.size()) {
      if (error) *error = "invalid ZIP: central directory entry out of bounds";
      return false;
    }

    const std::uint8_t* c = data.data() + offset;
    if (rd32(c) != 0x02014b50) {
      if (error) *error = "invalid ZIP: bad central directory signature";
      return false;
    }
    std::uint16_t method = rd16(c + 10);
    std::uint32_t comp_size = rd32(c + 20);
    std::uint16_t name_len = rd16(c + 28);
    std::uint16_t extra_len = rd16(c + 30);
    std::uint16_t comment_len = rd16(c + 32);
    std::uint32_t local_offset = rd32(c + 42);

    std::string name(reinterpret_cast<const char*>(data.data() + offset + 46), name_len);

    // Skip directories
    if (!name.empty() && name.back() == '/') {
      offset += 46 + name_len + extra_len + comment_len;
      continue;
    }

    // Read local header to find data offset (local fixed part is 30 bytes).
    std::uint32_t local_off = local_offset;
    if (local_off + 30 > data.size()) {
      if (error) *error = "invalid ZIP: local header out of bounds";
      return false;
    }
    const std::uint8_t* lh = data.data() + local_off;
    if (rd32(lh) != 0x04034b50) {
      if (error) *error = "invalid ZIP: bad local header signature";
      return false;
    }

    std::uint32_t data_off =
        local_off + 30 + rd16(lh + 26) + rd16(lh + 28);

    if (data_off + comp_size > data.size()) {
      if (error) *error = "invalid ZIP: compressed data out of bounds";
      return false;
    }

    std::vector<std::uint8_t> file_data;
    if (method == 0) {
      // Stored
      file_data.assign(data.begin() + data_off, data.begin() + data_off + comp_size);
    } else if (method == 8) {
      // Deflate
      if (!inflate_decompress(data.data() + data_off, comp_size, file_data)) {
        if (error) *error = "inflate failed for: " + name;
        return false;
      }
    } else {
      log_warn("updater", "unsupported ZIP compression method " + std::to_string(method) + " for " + name);
      offset += 46 + name_len + extra_len + comment_len;
      continue;
    }

    // Build output path
    std::string out_path = dest_dir;
    if (!out_path.empty() && out_path.back() != '/' && out_path.back() != '\\') {
      out_path += '/';
    }
    out_path += name;

    // Security: prevent path traversal
    if (out_path.find("..") != std::string::npos) {
      log_warn("updater", "skipping suspicious path: " + name);
      offset += 46 + name_len + extra_len + comment_len;
      continue;
    }

    if (!write_file(out_path, file_data)) {
      if (error) *error = "failed to write: " + out_path;
      return false;
    }

    offset += 46 + name_len + extra_len + comment_len;
  }

  return true;
}

}  // namespace wilfred
