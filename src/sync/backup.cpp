#include "wilfred/sync/backup.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/ipc/http.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <random>
#include <sstream>

namespace wilfred {
namespace fs = std::filesystem;

static constexpr char kMagic[] = "WILFBK1";
static constexpr char kEncMagic[] = "WILFEK1";
static constexpr std::size_t kSaltLen = 16;
static constexpr std::uint32_t kDefaultIters = 12000;

// Minimal in-tree SHA-256 (public domain style, no new deps).
struct BackupSha256 {
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  unsigned char buf[64]{};
  std::size_t buffered{0};
  std::uint64_t total{0};
  static std::uint32_t rotr(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
  void block(const unsigned char* p) {
    static const std::uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i)
      w[i] = (static_cast<std::uint32_t>(p[i * 4]) << 24) |
             (static_cast<std::uint32_t>(p[i * 4 + 1]) << 16) |
             (static_cast<std::uint32_t>(p[i * 4 + 2]) << 8) | static_cast<std::uint32_t>(p[i * 4 + 3]);
    for (int i = 16; i < 64; ++i) {
      auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
      auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
      w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
      auto S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
      auto ch = (e & f) ^ ((~e) & g);
      auto t1 = hh + S1 + ch + K[i] + w[i];
      auto S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
      auto maj = (a & b) ^ (a & c) ^ (b & c);
      auto t2 = S0 + maj;
      hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
  }
  void update(const unsigned char* data, std::size_t n) {
    total += n;
    while (n) {
      std::size_t take = 64 - buffered;
      if (take > n) take = n;
      std::memcpy(buf + buffered, data, take);
      buffered += take; data += take; n -= take;
      if (buffered == 64) { block(buf); buffered = 0; }
    }
  }
  void update(const std::string& s) {
    update(reinterpret_cast<const unsigned char*>(s.data()), s.size());
  }
  std::string digest() {
    unsigned char pad[64]{};
    pad[0] = 0x80;
    std::uint64_t bits = total * 8;
    std::size_t tail = total % 64;
    std::size_t first = tail < 56 ? 56 - tail : 120 - tail;
    // Pad without altering total for length encoding below.
    std::uint64_t saved = total;
    std::size_t saved_buf = buffered;
    update(pad, first);
    for (int i = 0; i < 8; ++i) {
      buf[buffered++] = static_cast<unsigned char>((bits >> ((7 - i) * 8)) & 0xff);
      if (buffered == 64) { block(buf); buffered = 0; }
    }
    (void)saved; (void)saved_buf;
    std::string o(32, '\0');
    for (int i = 0; i < 8; ++i) {
      o[i * 4] = static_cast<char>((h[i] >> 24) & 0xff);
      o[i * 4 + 1] = static_cast<char>((h[i] >> 16) & 0xff);
      o[i * 4 + 2] = static_cast<char>((h[i] >> 8) & 0xff);
      o[i * 4 + 3] = static_cast<char>(h[i] & 0xff);
    }
    return o;
  }
};

static std::string backup_sha256(const std::string& s) {
  BackupSha256 h;
  h.update(s);
  return h.digest();
}

static std::string derive_backup_key(const std::string& password, const std::string& salt,
                                     std::uint32_t iters) {
  std::string key = backup_sha256(password + salt);
  for (std::uint32_t i = 1; i < iters; ++i) key = backup_sha256(key + password + salt);
  return key;
}

static void xor_keystream(std::string& data, const std::string& key, const std::string& salt) {
  std::uint64_t blocks = (data.size() + 31) / 32;
  for (std::uint64_t b = 0; b < blocks; ++b) {
    std::string ctr(8, '\0');
    for (int i = 0; i < 8; ++i) ctr[i] = static_cast<char>((b >> (i * 8)) & 0xff);
    auto ks = backup_sha256(key + salt + ctr);
    std::size_t off = static_cast<std::size_t>(b * 32);
    for (std::size_t i = 0; i < 32 && off + i < data.size(); ++i)
      data[off + i] ^= ks[i];
  }
}

static std::string random_salt() {
  std::string s(kSaltLen, '\0');
  std::random_device rd;
  auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
  std::uint64_t seed = static_cast<std::uint64_t>(now) ^ (static_cast<std::uint64_t>(rd()) << 32 | rd());
  std::mt19937_64 rng(seed);
  for (auto& c : s) c = static_cast<char>(rng() & 0xff);
  return s;
}

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

bool resolve_sync_password(const Config& cfg, std::string& out, std::string& err) {
  out.clear();
  if (!cfg.sync.password.empty()) {
    out = cfg.sync.password;
    return true;
  }
  if (!cfg.sync.key_file.empty()) {
    if (!read_file_all(cfg.sync.key_file, out)) {
      err = "unable to read sync.key_file";
      return false;
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    if (out.empty()) {
      err = "sync.key_file is empty";
      return false;
    }
    return true;
  }
  err = "sync.encrypt needs sync.password or sync.key_file";
  return false;
}

bool pack_backup_archive_encrypted(const std::vector<BackupEntry>& files,
                                   const std::string& password, std::string& out,
                                   std::string& err) {
  if (password.empty()) {
    err = "empty sync password";
    return false;
  }
  std::string plain;
  if (!pack_backup_archive(files, plain, err)) return false;
  std::string salt = random_salt();
  auto key = derive_backup_key(password, salt, kDefaultIters);
  std::string enc = plain;
  xor_keystream(enc, key, salt);
  out.clear();
  out.append(kEncMagic, 7);
  out.append(salt);
  put_u32(out, kDefaultIters);
  put_u64(out, plain.size());
  put_u32(out, crc32(plain.data(), plain.size()));
  out.append(enc);
  return true;
}

bool unpack_backup_archive_auto(const std::string& blob, const std::string& password,
                                std::vector<BackupEntry>& files, std::string& err) {
  files.clear();
  if (blob.size() >= 7 && blob.compare(0, 7, kMagic) == 0)
    return unpack_backup_archive(blob, files, err);
  if (blob.size() < 7 || blob.compare(0, 7, kEncMagic) != 0) {
    err = "not a Wilfred backup archive";
    return false;
  }
  if (password.empty()) {
    err = "backup is encrypted: set sync.password or sync.key_file to restore";
    return false;
  }
  std::size_t i = 7;
  if (blob.size() < 7 + kSaltLen + 4 + 8 + 4) {
    err = "corrupt encrypted backup header";
    return false;
  }
  std::string salt = blob.substr(i, kSaltLen);
  i += kSaltLen;
  std::uint32_t iters = 0;
  std::uint64_t plain_len = 0;
  std::uint32_t crc = 0;
  if (!get_u32(blob, i, iters) || !get_u64(blob, i, plain_len) || !get_u32(blob, i, crc)) {
    err = "corrupt encrypted backup header";
    return false;
  }
  if (iters < 1000 || iters > 500000 || plain_len > 512 * 1024 * 1024) {
    err = "corrupt encrypted backup header";
    return false;
  }
  if (i + (blob.size() - i) < plain_len || blob.size() - i != plain_len) {
    err = "truncated encrypted backup data";
    return false;
  }
  std::string enc = blob.substr(i);
  auto key = derive_backup_key(password, salt, iters);
  xor_keystream(enc, key, salt);
  if (crc32(enc.data(), enc.size()) != crc) {
    err = "wrong password or corrupt backup";
    return false;
  }
  return unpack_backup_archive(enc, files, err);
}

static bool restore_entries(const std::vector<BackupEntry>& files, std::string& err) {
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

bool restore_backup(const std::string& src_path, std::string& err) {
  std::string blob;
  if (!read_file_all(src_path, blob)) {
    err = "unable to read " + src_path;
    return false;
  }
  // Plain restores keep working; encrypted ones report the password hint.
  if (blob.size() >= 7 && blob.compare(0, 7, kEncMagic) == 0) {
    err = "backup is encrypted: use restore with sync.password (see sync.encrypt)";
    return false;
  }
  std::vector<BackupEntry> files;
  if (!unpack_backup_archive(blob, files, err)) return false;
  return restore_entries(files, err);
}

bool restore_backup_with_password(const std::string& src_path, const std::string& password,
                                  std::string& err) {
  std::string blob;
  if (!read_file_all(src_path, blob)) {
    err = "unable to read " + src_path;
    return false;
  }
  std::vector<BackupEntry> files;
  if (!unpack_backup_archive_auto(blob, password, files, err)) return false;
  return restore_entries(files, err);
}

bool sync_push(const Config& cfg, std::string& err) {
  if (cfg.sync.url.empty()) {
    err = "sync.url is empty";
    return false;
  }
  // Collect entries directly so encryption can wrap the packed blob.
  std::vector<BackupEntry> files;
  auto cfg_path = cfg.source_path.empty() ? default_config_path() : cfg.source_path;
  auto add_one = [&](const std::string& disk, const std::string& name) {
    if (!file_exists(disk)) return;
    BackupEntry e;
    e.name = name;
    if (read_file_all(disk, e.data)) files.push_back(std::move(e));
  };
  add_one(cfg_path, "wilfred.yml");
  add_one(path_join(config_directory(), "snippets.yml"), "snippets.yml");
  add_one(default_history_path(), "history.bin");
  if (cfg.sync.include_index) add_tree(files, default_index_path(), "index");
  std::string blob;
  if (cfg.sync.encrypt) {
    std::string pw, perr;
    if (!resolve_sync_password(cfg, pw, perr)) {
      err = perr;
      return false;
    }
    if (!pack_backup_archive_encrypted(files, pw, blob, err)) return false;
  } else {
    if (!pack_backup_archive(files, blob, err)) return false;
  }
  auto tmp = default_backup_path() + ".push";
  create_directories(path_parent(tmp));
  if (!write_file_atomic(tmp, blob.data(), blob.size())) {
    err = "unable to write temp backup";
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
  std::string pw;
  if (cfg.sync.encrypt) {
    if (!resolve_sync_password(cfg, pw, err)) return false;
  }
  // Auto-detect plain vs encrypted so mixed fleets keep working.
  if (!pw.empty()) return restore_backup_with_password(tmp, pw, err);
  std::vector<BackupEntry> files;
  if (blob.size() >= 7 && blob.compare(0, 7, kEncMagic) == 0) {
    err = "remote backup is encrypted: set sync.password to pull";
    return false;
  }
  return restore_backup(tmp, err);
}

}  // namespace wilfred
