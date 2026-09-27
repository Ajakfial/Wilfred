#include "wilfred/sync/backup.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/ipc/http.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace wilfred {
namespace fs = std::filesystem;

static constexpr char kMagic[] = "WILFBK1";

static void put_u32(std::string& o, std::uint32_t v) {
  o.push_back(static_cast<char>(v & 0xff));
  o.push_back(static_cast<char>((v >> 8) & 0xff));
  o.push_back(static_cast<char>((v >> 16) & 0xff));
  o.push_back(static_cast<char>((v >> 24) & 0xff));
}

static void put_u64(std::string& o, std::uint64_t v) {
  put_u32(o, static_cast<std::uint32_t>(v));
  put_u32(o, static_cast<std::uint32_t>(v >> 32));
}

static bool get_u32(const std::string& s, std::size_t& i, std::uint32_t& v) {
  if (i + 4 > s.size()) return false;
  v = static_cast<std::uint8_t>(s[i]) | (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[i + 1])) << 8) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[i + 2])) << 16) |
      (static_cast<std::uint32_t>(static_cast<std::uint8_t>(s[i + 3])) << 24);
  i += 4;
  return true;
}

static bool get_u64(const std::string& s, std::size_t& i, std::uint64_t& v) {
  std::uint32_t lo = 0, hi = 0;
  if (!get_u32(s, i, lo) || !get_u32(s, i, hi)) return false;
  v = lo | (static_cast<std::uint64_t>(hi) << 32);
  return true;
}

bool pack_backup_archive(const std::vector<BackupEntry>& files, std::string& out, std::string& err) {
  out.clear();
  out.append(kMagic, 7);
  put_u32(out, static_cast<std::uint32_t>(files.size()));
  for (auto& f : files) {
    put_u32(out, static_cast<std::uint32_t>(f.name.size()));
    out.append(f.name);
    put_u64(out, f.data.size());
    put_u32(out, crc32(f.data.data(), f.data.size()));
    out.append(f.data);
  }
  (void)err;
  return true;
}

bool unpack_backup_archive(const std::string& blob, std::vector<BackupEntry>& files, std::string& err) {
  files.clear();
  if (blob.size() < 11 || blob.compare(0, 7, kMagic) != 0) {
    err = "not a Wilfred backup archive";
    return false;
  }
  std::size_t i = 7;
  std::uint32_t n = 0;
  if (!get_u32(blob, i, n) || n > 10000) {
    err = "corrupt backup header";
    return false;
  }
  for (std::uint32_t k = 0; k < n; ++k) {
    std::uint32_t nlen = 0;
    if (!get_u32(blob, i, nlen) || i + nlen > blob.size()) {
      err = "corrupt entry name";
      return false;
    }
    BackupEntry e;
    e.name = blob.substr(i, nlen);
    i += nlen;
    std::uint64_t dlen = 0;
    std::uint32_t crc = 0;
    if (!get_u64(blob, i, dlen) || !get_u32(blob, i, crc)) {
      err = "corrupt entry header";
      return false;
    }
    if (i + dlen > blob.size()) {
      err = "truncated backup data";
      return false;
    }
    e.data = blob.substr(i, static_cast<std::size_t>(dlen));
    i += static_cast<std::size_t>(dlen);
    if (crc32(e.data.data(), e.data.size()) != crc) {
      err = "checksum mismatch for " + e.name;
      return false;
    }
    files.push_back(std::move(e));
  }
  return true;
}

static bool add_file(std::vector<BackupEntry>& files, const std::string& disk, const std::string& name) {
  if (!file_exists(disk)) return true;
  BackupEntry e;
  e.name = name;
  if (!read_file_all(disk, e.data)) return false;
  files.push_back(std::move(e));
  return true;
}

static bool add_tree(std::vector<BackupEntry>& files, const std::string& dir, const std::string& prefix) {
  std::error_code ec;
  fs::path root = fs::u8path(dir);
  if (!fs::exists(root, ec)) return true;
  for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (ec || !it->is_regular_file(ec)) continue;
    auto rel = fs::relative(it->path(), root, ec);
    if (ec) continue;
    auto rels = rel.generic_u8string();
    std::string name = prefix + "/" + std::string(rels.begin(), rels.end());
    BackupEntry e;
    e.name = name;
    auto p = it->path().u8string();
    if (!read_file_all(std::string(p.begin(), p.end()), e.data)) continue;
    files.push_back(std::move(e));
  }
  return true;
}

std::string default_backup_path() {
  auto dir = path_join(data_directory(), "backups");
  create_directories(dir);
  return path_join(dir, "wilfred.wilfbk");
}

bool create_backup(const Config& cfg, const std::string& dest_path, bool include_index, std::string& err) {
  std::vector<BackupEntry> files;
  auto cfg_path = cfg.source_path.empty() ? default_config_path() : cfg.source_path;
  add_file(files, cfg_path, "wilfred.yml");
  add_file(files, path_join(config_directory(), "snippets.yml"), "snippets.yml");
  add_file(files, default_history_path(), "history.bin");
  if (include_index) add_tree(files, default_index_path(), "index");
  std::string blob;
  if (!pack_backup_archive(files, blob, err)) return false;
  auto dest = dest_path.empty() ? default_backup_path() : dest_path;
  create_directories(path_parent(dest));
  if (!write_file_atomic(dest, blob.data(), blob.size())) {
    err = "unable to write " + dest;
    return false;
  }
  return true;
}

bool restore_backup(const std::string& src_path, std::string& err) {
  std::string blob;
  if (!read_file_all(src_path, blob)) {
    err = "unable to read " + src_path;
    return false;
  }
  std::vector<BackupEntry> files;
  if (!unpack_backup_archive(blob, files, err)) return false;
  for (auto& f : files) {
    std::string dest;
    if (f.name == "wilfred.yml")
      dest = default_config_path();
    else if (f.name == "snippets.yml")
      dest = path_join(config_directory(), "snippets.yml");
    else if (f.name == "history.bin")
      dest = default_history_path();
    else if (f.name.rfind("index/", 0) == 0)
      dest = path_join(default_index_path(), f.name.substr(6));
    else
      dest = path_join(data_directory(), f.name);
    create_directories(path_parent(dest));
    if (!write_file_atomic(dest, f.data.data(), f.data.size())) {
      err = "unable to restore " + dest;
      return false;
    }
  }
  return true;
}

bool sync_push(const Config& cfg, std::string& err) {
  if (cfg.sync.url.empty()) {
    err = "sync.url is empty";
    return false;
  }
  auto tmp = default_backup_path() + ".push";
  if (!create_backup(cfg, tmp, cfg.sync.include_index, err)) return false;
  std::string blob;
  if (!read_file_all(tmp, blob)) {
    err = "unable to read backup";
    return false;
  }
  return http_put(cfg.sync.url, cfg.sync.token, blob, 20000, &err);
}

bool sync_pull(const Config& cfg, std::string& err) {
  if (cfg.sync.url.empty()) {
    err = "sync.url is empty";
    return false;
  }
  auto blob = http_get(cfg.sync.url, cfg.sync.token, 20000);
  if (blob.empty()) {
    err = "empty response from sync url";
    return false;
  }
  auto tmp = default_backup_path() + ".pull";
  if (!write_file_atomic(tmp, blob.data(), blob.size())) {
    err = "unable to write temp backup";
    return false;
  }
  return restore_backup(tmp, err);
}

}  // namespace wilfred
