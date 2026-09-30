// Minimal tar.gz archive extractor.
// Handles gzip decompression followed by tar parsing.

#include "wilfred/updater/updater.hpp"
#include "wilfred/updater/inflate.hpp"

#include "wilfred/core/log.hpp"

#include <cstring>
#include <fstream>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {

namespace {

struct TarHeader {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char padding[12];
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

// Parse an octal string (tar numeric fields)
std::uint64_t parse_octal(const char* s, std::size_t len) {
  std::uint64_t result = 0;
  for (std::size_t i = 0; i < len; ++i) {
    char c = s[i];
    if (c == ' ' || c == '\0') continue;
    if (c < '0' || c > '7') break;
    result = result * 8 + static_cast<std::uint64_t>(c - '0');
  }
  return result;
}

}  // namespace

bool extract_tarball(const std::string& archive_path, const std::string& dest_dir,
                     std::string* error) {
  std::vector<std::uint8_t> compressed;
  if (!read_file(archive_path, compressed)) {
    if (error) *error = "cannot read archive: " + archive_path;
    return false;
  }

  if (compressed.size() < 18) {
    if (error) *error = "archive too small";
    return false;
  }

  // Decompress gzip
  std::vector<std::uint8_t> tar_data;
  if (!inflate_decompress(compressed.data(), compressed.size(), tar_data)) {
    if (error) *error = "gzip decompression failed";
    return false;
  }

  std::size_t pos = 0;
  while (pos + 512 <= tar_data.size()) {
    TarHeader hdr;
    std::memcpy(&hdr, tar_data.data() + pos, sizeof(TarHeader));

    // Check for end-of-archive (two consecutive zero blocks)
    bool all_zero = true;
    for (std::size_t i = 0; i < 512; ++i) {
      if (tar_data[pos + i] != 0) {
        all_zero = false;
        break;
      }
    }
    if (all_zero) break;

    // Validate magic
    if (std::memcmp(hdr.magic, "ustar", 5) != 0) {
      // Try GNU tar (no magic)
      if (hdr.name[0] == '\0') break;
    }

    std::uint64_t file_size = parse_octal(hdr.size, sizeof(hdr.size));
    std::string name(hdr.name, strnlen(hdr.name, sizeof(hdr.name)));

    // Prepend prefix if present (POSIX)
    std::string prefix(hdr.prefix, strnlen(hdr.prefix, sizeof(hdr.prefix)));
    if (!prefix.empty()) {
      name = prefix + "/" + name;
    }

    pos += 512;  // advance past header

    char type = hdr.typeflag;
    if (type == '0' || type == '\0') {
      // Regular file
      if (!name.empty() && name.back() != '/') {
        std::string out_path = dest_dir;
        if (!out_path.empty() && out_path.back() != '/' && out_path.back() != '\\') {
          out_path += '/';
        }
        out_path += name;

        // Security: prevent path traversal
        if (out_path.find("..") != std::string::npos) {
          log_warn("updater", "skipping suspicious path: " + name);
        } else {
          std::vector<std::uint8_t> file_data(tar_data.begin() + pos,
                                              tar_data.begin() + pos + file_size);
          if (!write_file(out_path, file_data)) {
            if (error) *error = "failed to write: " + out_path;
            return false;
          }
        }
      }
    } else if (type == '5') {
      // Directory - create it
      if (!name.empty()) {
        std::string dir_path = dest_dir;
        if (!dir_path.empty() && dir_path.back() != '/' && dir_path.back() != '\\') {
          dir_path += '/';
        }
        dir_path += name;
#ifdef _WIN32
        CreateDirectoryA(dir_path.c_str(), nullptr);
#else
        mkdir(dir_path.c_str(), 0755);
#endif
      }
    }
    // Skip other types (symlinks, devices, etc.)

    // Advance past file data (padded to 512-byte boundary)
    pos += ((file_size + 511) / 512) * 512;
  }

  return true;
}

}  // namespace wilfred
