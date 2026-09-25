#include "wilfred/index/wal.hpp"

#include "wilfred/core/crc32.hpp"
#include "wilfred/core/utf8.hpp"

#include <cstring>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace wilfred {

static bool write_u32(FILE* f, std::uint32_t v) {
  unsigned char b[4] = {static_cast<unsigned char>(v), static_cast<unsigned char>(v >> 8),
                        static_cast<unsigned char>(v >> 16), static_cast<unsigned char>(v >> 24)};
  return std::fwrite(b, 1, 4, f) == 4;
}
static bool write_u64(FILE* f, std::uint64_t v) {
  return write_u32(f, static_cast<std::uint32_t>(v)) &&
         write_u32(f, static_cast<std::uint32_t>(v >> 32));
}
static bool write_i64(FILE* f, std::int64_t v) {
  return write_u64(f, static_cast<std::uint64_t>(v));
}
static bool write_str(FILE* f, const std::string& s) {
  if (!write_u32(f, static_cast<std::uint32_t>(s.size()))) return false;
  return s.empty() || std::fwrite(s.data(), 1, s.size(), f) == s.size();
}

static bool read_u32(FILE* f, std::uint32_t& v) {
  unsigned char b[4];
  if (std::fread(b, 1, 4, f) != 4) return false;
  v = static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) |
      (static_cast<std::uint32_t>(b[2]) << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
  return true;
}
static bool read_u64(FILE* f, std::uint64_t& v) {
  std::uint32_t lo = 0, hi = 0;
  if (!read_u32(f, lo) || !read_u32(f, hi)) return false;
  v = lo | (static_cast<std::uint64_t>(hi) << 32);
  return true;
}
static bool read_i64(FILE* f, std::int64_t& v) {
  std::uint64_t u = 0;
  if (!read_u64(f, u)) return false;
  v = static_cast<std::int64_t>(u);
  return true;
}
static bool read_str(FILE* f, std::string& s) {
  std::uint32_t n = 0;
  if (!read_u32(f, n)) return false;
  if (n > 16 * 1024 * 1024) return false;
  s.resize(n);
  return n == 0 || std::fread(s.data(), 1, n, f) == n;
}

static bool write_record(FILE* f, const IndexRecord& r) {
  return write_u32(f, r.id) && write_u32(f, r.name_id) && write_u32(f, r.path_id) &&
         write_u32(f, r.parent_id) && write_u32(f, r.ext_id) && write_u64(f, r.size) &&
         write_i64(f, r.ctime) && write_i64(f, r.mtime) && write_i64(f, r.atime) &&
         write_u32(f, static_cast<std::uint32_t>(r.flags)) && write_u32(f, r.volume_id) &&
         write_u32(f, static_cast<std::uint32_t>(r.kind)) && write_u32(f, r.mode);
}

static bool read_record(FILE* f, IndexRecord& r) {
  std::uint32_t flags = 0, vol = 0, kind = 0;
  if (!read_u32(f, r.id) || !read_u32(f, r.name_id) || !read_u32(f, r.path_id) ||
      !read_u32(f, r.parent_id) || !read_u32(f, r.ext_id) || !read_u64(f, r.size) ||
      !read_i64(f, r.ctime) || !read_i64(f, r.mtime) || !read_i64(f, r.atime) ||
      !read_u32(f, flags) || !read_u32(f, vol) || !read_u32(f, kind) || !read_u32(f, r.mode))
    return false;
  r.flags = static_cast<RecordFlags>(flags);
  r.volume_id = static_cast<std::uint16_t>(vol);
  r.kind = static_cast<FileKind>(kind);
  return true;
}

WriteAheadLog::~WriteAheadLog() { close(); }

bool WriteAheadLog::open(const std::string& path) {
  close();
  path_ = path;
#ifdef _WIN32
  auto w = utf8_to_wide(path);
  fp_ = _wfopen(w.c_str(), L"ab+");
#else
  fp_ = std::fopen(path.c_str(), "ab+");
#endif
  if (!fp_) return false;
  std::fseek(fp_, 0, SEEK_END);
  long s = std::ftell(fp_);
  size_ = s < 0 ? 0 : static_cast<std::uint64_t>(s);
  return true;
}

void WriteAheadLog::close() {
  if (fp_) {
    std::fflush(fp_);
    std::fclose(fp_);
    fp_ = nullptr;
  }
}

bool WriteAheadLog::write_all(const void* p, std::size_t n) {
  return std::fwrite(p, 1, n, fp_) == n;
}

bool WriteAheadLog::append_upsert(const IndexRecord& rec, const std::string& path) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!fp_) return false;
  unsigned char op = static_cast<unsigned char>(WalOp::Upsert);
  if (!write_all(&op, 1)) return false;
  if (!write_record(fp_, rec) || !write_str(fp_, path)) return false;
  std::fflush(fp_);
  size_ = static_cast<std::uint64_t>(std::ftell(fp_));
  return true;
}

bool WriteAheadLog::append_delete(const std::string& path) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!fp_) return false;
  unsigned char op = static_cast<unsigned char>(WalOp::Delete);
  if (!write_all(&op, 1) || !write_str(fp_, path)) return false;
  std::fflush(fp_);
  size_ = static_cast<std::uint64_t>(std::ftell(fp_));
  return true;
}

bool WriteAheadLog::append_rename(const std::string& from, const std::string& to) {
  std::lock_guard<std::mutex> lock(mu_);
  if (!fp_) return false;
  unsigned char op = static_cast<unsigned char>(WalOp::Rename);
  if (!write_all(&op, 1) || !write_str(fp_, from) || !write_str(fp_, to)) return false;
  std::fflush(fp_);
  size_ = static_cast<std::uint64_t>(std::ftell(fp_));
  return true;
}

bool WriteAheadLog::append_checkpoint() {
  std::lock_guard<std::mutex> lock(mu_);
  if (!fp_) return false;
  unsigned char op = static_cast<unsigned char>(WalOp::Checkpoint);
  if (!write_all(&op, 1)) return false;
  std::fflush(fp_);
  size_ = static_cast<std::uint64_t>(std::ftell(fp_));
  return true;
}

bool WriteAheadLog::replay(std::vector<WalEntry>& entries) {
  if (!fp_) return false;
  std::rewind(fp_);
  entries.clear();
  for (;;) {
    unsigned char opb = 0;
    if (std::fread(&opb, 1, 1, fp_) != 1) break;
    WalEntry e;
    e.op = static_cast<WalOp>(opb);
    if (e.op == WalOp::Upsert) {
      if (!read_record(fp_, e.record) || !read_str(fp_, e.path)) break;
    } else if (e.op == WalOp::Delete) {
      if (!read_str(fp_, e.path)) break;
    } else if (e.op == WalOp::Rename) {
      if (!read_str(fp_, e.path) || !read_str(fp_, e.new_path)) break;
    } else if (e.op == WalOp::Checkpoint) {
    } else {
      break;
    }
    entries.push_back(std::move(e));
  }
  return true;
}

bool WriteAheadLog::truncate() {
  close();
#ifdef _WIN32
  FILE* f = _wfopen(utf8_to_wide(path_).c_str(), L"wb");
#else
  FILE* f = std::fopen(path_.c_str(), "wb");
#endif
  if (!f) return false;
  std::fclose(f);
  size_ = 0;
  return open(path_);
}

}  // namespace wilfred
