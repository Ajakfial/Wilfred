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

struct ZipLocalHeader {
  std::uint32_t signature;    // 0x04034b50
  std::uint16_t version;
  std::uint16_t flags;
  std::uint16_t method;
  std::uint16_t mod_time;
  std::uint16_t mod_date;
  std::uint32_t crc32;
  std::uint32_t comp_size;
  std::uint32_t uncomp_size;
  std::uint16_t name_len;
  std::uint16_t extra_len;
};

struct ZipCentralEntry {
  std::uint32_t signature;    // 0x02014b50
  std::uint16_t version_made;
  std::uint16_t version_need;
  std::uint16_t flags;
  std::uint16_t method;
  std::uint16_t mod_time;
  std::uint16_t mod_date;
  std::uint32_t crc32;
  std::uint32_t comp_size;
  std::uint32_t uncomp_size;
  std::uint16_t name_len;
  std::uint16_t extra_len;
  std::uint16_t comment_len;
  std::uint16_t disk_start;
  std::uint16_t int_attr;
  std::uint32_t ext_attr;
  std::uint32_t local_offset;
};

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
  std::memcpy(&eocd, data.data() + eocd_pos, sizeof(eocd));
  // Byte-swap if needed (ZIP is little-endian, we assume LE host)
  // On LE hosts this is a no-op

  std::uint16_t total = eocd.total_entries;
  std::uint32_t offset = eocd.central_offset;

  for (std::uint16_t i = 0; i < total; ++i) {
    if (offset + 46 > data.size()) {
      if (error) *error = "invalid ZIP: central directory entry out of bounds";
      return false;
    }

    ZipCentralEntry entry;
    std::memcpy(&entry, data.data() + offset, sizeof(entry));

    if (entry.signature != 0x02014b50) {
      if (error) *error = "invalid ZIP: bad central directory signature";
      return false;
    }

    std::string name(reinterpret_cast<const char*>(data.data() + offset + 46), entry.name_len);

    // Skip directories
    if (!name.empty() && name.back() == '/') {
      offset += 46 + entry.name_len + entry.extra_len + entry.comment_len;
      continue;
    }

    // Read local header to find data offset
    std::uint32_t local_off = entry.local_offset;
    if (local_off + 30 > data.size()) {
      if (error) *error = "invalid ZIP: local header out of bounds";
      return false;
    }

    ZipLocalHeader lh;
    std::memcpy(&lh, data.data() + local_off, sizeof(lh));

    std::uint32_t data_off = local_off + 30 + lh.name_len + lh.extra_len;
    std::uint32_t comp_size = entry.comp_size;

    if (data_off + comp_size > data.size()) {
      if (error) *error = "invalid ZIP: compressed data out of bounds";
      return false;
    }

    std::vector<std::uint8_t> file_data;
    if (entry.method == 0) {
      // Stored
      file_data.assign(data.begin() + data_off, data.begin() + data_off + comp_size);
    } else if (entry.method == 8) {
      // Deflate
      if (!inflate_decompress(data.data() + data_off, comp_size, file_data)) {
        if (error) *error = "inflate failed for: " + name;
        return false;
      }
    } else {
      log_warn("updater", "unsupported ZIP compression method " + std::to_string(entry.method) + " for " + name);
      offset += 46 + entry.name_len + entry.extra_len + entry.comment_len;
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
      offset += 46 + entry.name_len + entry.extra_len + entry.comment_len;
      continue;
    }

    if (!write_file(out_path, file_data)) {
      if (error) *error = "failed to write: " + out_path;
      return false;
    }

    offset += 46 + entry.name_len + entry.extra_len + entry.comment_len;
  }

  return true;
}

}  // namespace wilfred
