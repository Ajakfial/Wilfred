#pragma once

#include "wilfred/index/record.hpp"

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <mutex>

namespace wilfred {

enum class WalOp : std::uint8_t {
  Upsert = 1,
  Delete = 2,
  Rename = 3,
  Checkpoint = 4,
};

struct WalEntry {
  WalOp op{WalOp::Upsert};
  IndexRecord record{};
  std::string path;
  std::string new_path;
};

class WriteAheadLog {
public:
  ~WriteAheadLog();
  bool open(const std::string& path);
  void close();
  bool append_upsert(const IndexRecord& rec, const std::string& path);
  bool append_delete(const std::string& path);
  bool append_rename(const std::string& from, const std::string& to);
  bool append_checkpoint();
  bool replay(std::vector<WalEntry>& entries);
  std::uint64_t size_bytes() const { return size_; }
  bool truncate();
  bool is_open() const { return fp_ != nullptr; }

private:
  bool write_all(const void* p, std::size_t n);
  FILE* fp_{nullptr};
  std::string path_;
  std::uint64_t size_{0};
  std::mutex mu_;
};

}  // namespace wilfred
