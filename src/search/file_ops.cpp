#include "wilfred/search/file_ops.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/paths.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <vector>

namespace wilfred {
namespace fs = std::filesystem;

std::string path_to_posix(const std::string& path) {
  std::string o = path;
  for (char& c : o)
    if (c == '\\') c = '/';
  return o;
}

static bool is_uri_unreserved(unsigned char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' ||
         c == '.' || c == '_' || c == '~';
}

std::string path_to_file_uri(const std::string& path) {
  if (path.empty()) return {};
  std::string posix = path_to_posix(path);
  // UNC \\server\share -> file://server/share
  if (posix.size() >= 2 && posix[0] == '/' && posix[1] == '/') {
    std::string rest = posix.substr(2);
    std::string o = "file://";
    for (unsigned char c : rest) {
      if (is_uri_unreserved(c) || c == '/' || c == ':')
        o.push_back(static_cast<char>(c));
      else {
        char buf[4];
        std::snprintf(buf, sizeof(buf), "%%%02X", c);
        o += buf;
      }
    }
    return o;
  }
  std::string o = "file://";
  if (!posix.empty() && posix[0] != '/') o.push_back('/');
  for (unsigned char c : posix) {
    if (is_uri_unreserved(c) || c == '/' || c == ':')
      o.push_back(static_cast<char>(c));
    else {
      char buf[4];
      std::snprintf(buf, sizeof(buf), "%%%02X", c);
      o += buf;
    }
  }
  // "file://" + "/abs" or "file://" + "/" + "rel"
  return o;
}

std::string path_to_wsl(const std::string& path) {
  if (path.size() < 3) return {};
  if (!((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z'))) return {};
  if (path[1] != ':' || (path[2] != '\\' && path[2] != '/')) return {};
  std::string o = "/mnt/";
  o.push_back(static_cast<char>(path[0] >= 'A' && path[0] <= 'Z' ? path[0] + 32 : path[0]));
  o += path_to_posix(path.substr(2));
  return o;
}

namespace {

struct Sha256 {
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
             (static_cast<std::uint32_t>(p[i * 4 + 2]) << 8) |
             static_cast<std::uint32_t>(p[i * 4 + 3]);
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
      hh = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    h[0] += a;
    h[1] += b;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
  }

  void update(const unsigned char* data, std::size_t n) {
    total += n;
    while (n) {
      std::size_t take = 64 - buffered;
      if (take > n) take = n;
      std::memcpy(buf + buffered, data, take);
      buffered += take;
      data += take;
      n -= take;
      if (buffered == 64) {
        block(buf);
        buffered = 0;
      }
    }
  }

  std::string final_hex() {
    unsigned char pad[64]{};
    pad[0] = 0x80;
    std::uint64_t bits = total * 8;
    std::size_t tail = total % 64;
    std::size_t first = tail < 56 ? 56 - tail : 120 - tail;
    update(pad, first);
    // Append the 8-byte big-endian length (exactly 8 bytes = no extra pad).
    for (int i = 0; i < 8; ++i) {
      buf[buffered++] = static_cast<unsigned char>((bits >> ((7 - i) * 8)) & 0xff);
      if (buffered == 64) {
        block(buf);
        buffered = 0;
      }
    }
    static const char* hex = "0123456789abcdef";
    std::string o;
    for (auto v : h) {
      for (int i = 3; i >= 0; --i) {
        o.push_back(hex[(v >> (i * 8 + 4)) & 0xf]);
        o.push_back(hex[(v >> (i * 8)) & 0xf]);
      }
    }
    return o;
  }
};

}  // namespace

bool sha256_file(const std::string& path, std::string& out_hex, std::string& error) {
  out_hex.clear();
  error.clear();
  std::error_code ec;
  auto st = fs::symlink_status(fs::u8path(path), ec);
  if (ec || !fs::is_regular_file(st)) {
    error = "Not a file";
    return false;
  }
  FILE* f = nullptr;
#ifdef _WIN32
  {
    std::wstring w = fs::u8path(path).wstring();
    if (_wfopen_s(&f, w.c_str(), L"rb") != 0) f = nullptr;
  }
#else
  f = std::fopen(path.c_str(), "rb");
#endif
  if (!f) {
    error = "Could not open file";
    return false;
  }
  Sha256 h;
  unsigned char chunk[65536];
  std::size_t n = 0;
  while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) {
    h.update(chunk, n);
  }
  bool read_err = std::ferror(f) != 0;
  std::fclose(f);
  if (read_err) {
    error = "Error reading file";
    return false;
  }
  out_hex = h.final_hex();
  return true;
}

namespace {

void put_le16(std::string& o, std::uint16_t v) {
  o.push_back(static_cast<char>(v & 0xff));
  o.push_back(static_cast<char>((v >> 8) & 0xff));
}

void put_le32(std::string& o, std::uint32_t v) {
  for (int i = 0; i < 4; ++i)
    o.push_back(static_cast<char>((v >> (i * 8)) & 0xff));
}

static std::string fs_to_utf8(const fs::path& p) {
  auto u8 = p.u8string();
  return std::string(u8.begin(), u8.end());
}

void dos_datetime(std::uint16_t& dos_date, std::uint16_t& dos_time) {
  std::time_t t = std::time(nullptr);
  std::tm tmv{};
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  int year = tmv.tm_year + 1900;
  if (year < 1980) year = 1980;
  dos_date =
      static_cast<std::uint16_t>(((year - 1980) << 9) | ((tmv.tm_mon + 1) << 5) | tmv.tm_mday);
  dos_time = static_cast<std::uint16_t>((tmv.tm_hour << 11) | (tmv.tm_min << 5) | (tmv.tm_sec / 2));
}

struct ZipEntry {
  std::string name;
  std::uint32_t crc{0};
  std::uint32_t size{0};
  std::uint64_t local_offset{0};
  bool is_dir{false};
};

bool append_file_bytes(const std::string& path, std::string& blob, std::uint32_t& crc,
                       std::uint32_t& size, std::string& error) {
  crc = 0;
  size = 0;
  FILE* f = nullptr;
#ifdef _WIN32
  {
    std::wstring w = fs::u8path(path).wstring();
    if (_wfopen_s(&f, w.c_str(), L"rb") != 0) f = nullptr;
  }
#else
  f = std::fopen(path.c_str(), "rb");
#endif
  if (!f) {
    error = "Could not open " + path;
    return false;
  }
  unsigned char chunk[65536];
  std::size_t n = 0;
  std::uint64_t total = 0;
  while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) {
    total += n;
    if (total > 0xFFFFFFFFull) {
      std::fclose(f);
      error = "File too large for zip: " + path;
      return false;
    }
    crc = crc32(chunk, n, crc);
    blob.append(reinterpret_cast<const char*>(chunk), n);
  }
  bool read_err = std::ferror(f) != 0;
  std::fclose(f);
  if (read_err) {
    error = "Error reading " + path;
    return false;
  }
  size = static_cast<std::uint32_t>(total);
  return true;
}

}  // namespace

bool zip_paths_to(const std::vector<std::string>& sources, const std::string& zip_path,
                  std::string& error) {
  error.clear();
  std::uint16_t dos_date = 0, dos_time = 0;
  dos_datetime(dos_date, dos_time);

  // Collect entries first so directory inputs expand deterministically.
  struct Job {
    std::string fs_path;
    std::string arc_name;
    bool is_dir{false};
  };
  std::vector<Job> jobs;
  for (auto& src : sources) {
    std::error_code ec;
    auto st = fs::symlink_status(fs::u8path(src), ec);
    if (ec) continue;
    if (fs::is_directory(st)) {
      std::string base = path_filename(src);
      if (base.empty()) base = "folder";
      for (auto it = fs::recursive_directory_iterator(fs::u8path(src), ec);
           it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) break;
        auto p = it->path();
        auto rel = p.lexically_relative(fs::u8path(src));
        std::string rel8 = fs_to_utf8(rel);
        for (char& c : rel8)
          if (c == '\\') c = '/';
        std::string arc = base + "/" + rel8;
        auto sst = it->symlink_status(ec);
        if (ec) continue;
        if (it->is_symlink(ec)) continue;
        if (it->is_directory(ec))
          jobs.push_back({fs_to_utf8(p), arc + "/", true});
        else if (it->is_regular_file(ec))
          jobs.push_back({fs_to_utf8(p), arc, false});
        if (jobs.size() >= 100000) break;
      }
      if (jobs.empty()) {
        // Empty directory: record the folder itself.
        jobs.push_back({src, base + "/", true});
      }
    } else if (fs::is_regular_file(st)) {
      std::string base = path_filename(src);
      if (base.empty()) base = "file";
      jobs.push_back({src, base, false});
    }
    if (jobs.size() >= 100000) break;
  }
  if (jobs.empty()) {
    error = "Nothing to compress";
    return false;
  }

  std::string blob;
  std::vector<ZipEntry> entries;
  entries.reserve(jobs.size());
  for (auto& j : jobs) {
    ZipEntry e;
    e.name = j.arc_name;
    e.is_dir = j.is_dir;
    e.local_offset = blob.size();
    std::string local;
    put_le32(local, 0x04034b50u);
    put_le16(local, 20);       // version needed
    put_le16(local, 0x0800u);  // UTF-8 flag
    put_le16(local, 0);        // stored
    put_le16(local, dos_time);
    put_le16(local, dos_date);
    if (!j.is_dir) {
      std::string data;
      if (!append_file_bytes(j.fs_path, data, e.crc, e.size, error)) return false;
      put_le32(local, e.crc);
      put_le32(local, e.size);
      put_le32(local, e.size);
      put_le16(local, static_cast<std::uint16_t>(e.name.size()));
      put_le16(local, 0);
      blob += local;
      blob += e.name;
      blob += data;
    } else {
      put_le32(local, 0);
      put_le32(local, 0);
      put_le32(local, 0);
      put_le16(local, static_cast<std::uint16_t>(e.name.size()));
      put_le16(local, 0);
      blob += local;
      blob += e.name;
    }
    entries.push_back(e);
  }

  std::uint64_t central_offset = blob.size();
  std::string central;
  for (auto& e : entries) {
    put_le32(central, 0x02014b50u);
    put_le16(central, 20);       // version made by
    put_le16(central, 20);       // version needed
    put_le16(central, 0x0800u);  // UTF-8 flag
    put_le16(central, 0);        // stored
    put_le16(central, dos_time);
    put_le16(central, dos_date);
    put_le32(central, e.crc);
    put_le32(central, e.size);
    put_le32(central, e.size);
    put_le16(central, static_cast<std::uint16_t>(e.name.size()));
    put_le16(central, 0);
    put_le16(central, 0);
    put_le16(central, 0);
    put_le16(central, 0);
    put_le32(central, e.is_dir ? (0x10u << 16) : 0);
    put_le32(central, static_cast<std::uint32_t>(e.local_offset));
    central += e.name;
  }
  std::uint64_t dir_size = central.size();
  blob += central;
  std::string eocd;
  put_le32(eocd, 0x06054b50u);
  put_le16(eocd, 0);
  put_le16(eocd, 0);
  put_le16(eocd, static_cast<std::uint16_t>(entries.size()));
  put_le16(eocd, static_cast<std::uint16_t>(entries.size()));
  put_le32(eocd, static_cast<std::uint32_t>(dir_size));
  put_le32(eocd, static_cast<std::uint32_t>(central_offset));
  put_le16(eocd, 0);
  blob += eocd;

  FILE* f = nullptr;
#ifdef _WIN32
  {
    std::wstring w = fs::u8path(zip_path).wstring();
    if (_wfopen_s(&f, w.c_str(), L"wb") != 0) f = nullptr;
  }
#else
  f = std::fopen(zip_path.c_str(), "wb");
#endif
  if (!f) {
    error = "Could not write " + zip_path;
    return false;
  }
  bool ok = std::fwrite(blob.data(), 1, blob.size(), f) == blob.size();
  std::fclose(f);
  if (!ok) error = "Could not write " + zip_path;
  return ok;
}

std::string unique_sibling_path(const std::string& dir, const std::string& stem,
                                const std::string& ext) {
  std::error_code ec;
  auto first = path_join(dir, stem + ext);
  if (!fs::exists(fs::u8path(first), ec)) return first;
  for (int i = 2; i < 10000; ++i) {
    auto cand = path_join(dir, stem + " " + std::to_string(i) + ext);
    if (!fs::exists(fs::u8path(cand), ec)) return cand;
  }
  return path_join(dir, stem + ext);
}

bool create_new_file_here(const std::string& dir, std::string& out_path, std::string& error) {
  out_path.clear();
  error.clear();
  std::error_code ec;
  fs::create_directories(fs::u8path(dir), ec);
  auto p = unique_sibling_path(dir, "New file", ".txt");
  FILE* f = nullptr;
#ifdef _WIN32
  {
    std::wstring w = fs::u8path(p).wstring();
    if (_wfopen_s(&f, w.c_str(), L"wb") != 0) f = nullptr;
  }
#else
  f = std::fopen(p.c_str(), "wb");
#endif
  if (!f) {
    error = "Could not create file";
    return false;
  }
  std::fclose(f);
  out_path = p;
  return true;
}

bool create_new_folder_here(const std::string& dir, std::string& out_path, std::string& error) {
  out_path.clear();
  error.clear();
  std::error_code ec;
  fs::create_directories(fs::u8path(dir), ec);
  for (int i = 1; i < 10000; ++i) {
    auto p =
        i == 1 ? path_join(dir, "New folder") : path_join(dir, "New folder " + std::to_string(i));
    ec.clear();
    bool created = fs::create_directory(fs::u8path(p), ec);
    if (created && !ec) {
      out_path = p;
      return true;
    }
  }
  error = "Could not create folder";
  return false;
}

bool fs_is_directory(const std::string& path) {
  std::error_code ec;
  return fs::is_directory(fs::u8path(path), ec);
}

bool fs_exists(const std::string& path) {
  std::error_code ec;
  return fs::exists(fs::u8path(path), ec);
}

bool bulk_rename_in_dir(const std::string& dir, const std::string& pattern,
                        std::vector<std::string>& renamed_out, std::string& error) {
  renamed_out.clear();
  error.clear();
  if (pattern.empty() || pattern.size() > 128) {
    error = "rename pattern is empty (try: photo-{n}.jpg with {n} {name} {ext})";
    return false;
  }
  std::error_code ec;
  if (!fs::is_directory(fs::u8path(dir), ec)) {
    error = "Not a directory";
    return false;
  }
  std::vector<std::string> files;
  for (auto it = fs::directory_iterator(fs::u8path(dir), ec); it != fs::directory_iterator();
       it.increment(ec)) {
    if (ec) break;
    std::error_code ec2;
    if (it->is_regular_file(ec2)) {
      auto u8 = it->path().u8string();
      files.emplace_back(std::string(u8.begin(), u8.end()));
    }
  }
  if (files.empty()) {
    error = "No files to rename";
    return false;
  }
  std::sort(files.begin(), files.end());
  int n = 1;
  std::vector<std::pair<std::string, std::string>> jobs;
  for (auto& f : files) {
    std::string stem = path_stem(f);
    std::string ext = path_extension(f);
    std::string name = pattern;
    auto rep = [&](const std::string& key, const std::string& val) {
      std::size_t pos = 0;
      while ((pos = name.find(key, pos)) != std::string::npos) {
        name.replace(pos, key.size(), val);
        pos += val.size();
      }
    };
    rep("{n}", std::to_string(n));
    rep("{name}", stem);
    rep("{ext}", ext);
    if (name.find('.') == std::string::npos && !ext.empty()) name += ext;
    auto dest = path_join(dir, name);
    if (dest == f) {
      ++n;
      continue;
    }
    if (fs::exists(fs::u8path(dest), ec)) {
      error = "Target exists: " + name + " (rename aborted, nothing changed)";
      return false;
    }
    jobs.emplace_back(f, dest);
    ++n;
  }
  if (jobs.empty()) {
    error = "Nothing to rename";
    return false;
  }
  for (auto& [src, dst] : jobs) {
    std::error_code ec2;
    fs::rename(fs::u8path(src), fs::u8path(dst), ec2);
    if (ec2) {
      error = "Rename failed: " + src + " -> " + dst;
      return false;
    }
    renamed_out.push_back(dst);
  }
  return true;
}

bool move_paths_to(const std::vector<std::string>& sources, const std::string& dest_dir,
                   std::vector<std::string>& moved_out, std::string& error) {
  moved_out.clear();
  error.clear();
  if (dest_dir.empty()) {
    error = "Destination is empty";
    return false;
  }
  std::error_code ec;
  fs::create_directories(fs::u8path(dest_dir), ec);
  if (ec || !fs::is_directory(fs::u8path(dest_dir), ec)) {
    error = "Cannot create destination";
    return false;
  }
  for (auto& s : sources) {
    if (s.empty() || !fs_exists(s)) {
      error = "No such file: " + s;
      return false;
    }
    auto base = path_filename(s);
    if (base.empty()) base = "file";
    auto dest = path_join(dest_dir, base);
    if (fs::exists(fs::u8path(dest), ec)) {
      error = "Target exists: " + dest;
      return false;
    }
    std::error_code ec2;
    fs::rename(fs::u8path(s), fs::u8path(dest), ec2);
    if (ec2) {
      error = "Move failed: " + s;
      return false;
    }
    moved_out.push_back(dest);
  }
  return !moved_out.empty();
}

bool create_from_template(const std::string& dir, const std::string& template_name,
                          const std::string& new_name, std::string& out_path, std::string& error) {
  out_path.clear();
  error.clear();
  std::string t = template_name;
  for (char& c : t)
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  std::string body;
  std::string ext = ".txt";
  if (t.empty() || t == "empty" || t == "txt" || t == "text") {
    body = "";
  } else if (t == "md" || t == "markdown" || t == "note") {
    ext = ".md";
    body = "# Title\n\n";
  } else if (t == "py" || t == "python") {
    ext = ".py";
    body =
        "#!/usr/bin/env python3\n\"\"\"Module.\"\"\"\n\n\ndef main():\n    pass\n\n\nif __name__ "
        "== \"__main__\":\n    main()\n";
  } else if (t == "cpp" || t == "c++") {
    ext = ".cpp";
    body = "#include <iostream>\n\nint main() {\n  std::cout << \"hi\\n\";\n}\n";
  } else if (t == "h" || t == "header") {
    ext = ".hpp";
    body = "#pragma once\n\n";
  } else if (t == "html") {
    ext = ".html";
    body =
        "<!doctype html>\n<html>\n<head><meta "
        "charset=\"utf-8\"><title>Doc</title></head>\n<body></body>\n</html>\n";
  } else if (t == "json") {
    ext = ".json";
    body = "{\n  \"key\": \"value\"\n}\n";
  } else if (t == "gitignore" || t == "ignore") {
    ext = "";
    body = "build/\n*.o\nnode_modules/\n.DS_Store\n";
  } else if (t == "todo" || t == "tasks") {
    ext = ".md";
    body = "# Todos\n\n- [ ] first task\n";
  } else {
    error = "Unknown template '" + template_name +
            "' (try: empty, md, python, cpp, html, json, gitignore)";
    return false;
  }
  std::string name = new_name.empty() ? "New file" : new_name;
  if (t == "gitignore")
    name = ".gitignore";
  else if (name.find('.') == std::string::npos)
    name += ext;
  std::error_code ec;
  fs::create_directories(fs::u8path(dir), ec);
  auto dest = path_join(dir, name);
  if (fs::exists(fs::u8path(dest), ec)) {
    auto stem = path_stem(dest);
    auto e2 = path_extension(dest);
    dest = unique_sibling_path(dir, stem.empty() ? name : stem, e2);
  }
#ifdef _WIN32
  FILE* f = nullptr;
  {
    std::wstring w = fs::u8path(dest).wstring();
    if (_wfopen_s(&f, w.c_str(), L"wb") != 0) f = nullptr;
  }
#else
  FILE* f = std::fopen(dest.c_str(), "wb");
#endif
  if (!f) {
    error = "Could not create file";
    return false;
  }
  if (!body.empty()) std::fwrite(body.data(), 1, body.size(), f);
  std::fclose(f);
  out_path = dest;
  return true;
}

}  // namespace wilfred
