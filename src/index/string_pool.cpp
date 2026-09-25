#include "wilfred/index/string_pool.hpp"

#include <cstring>

namespace wilfred {

void StringPool::rebuild_map() {
  map_.clear();
  map_.reserve(offsets_.size());
  for (std::uint32_t i = 0; i < offsets_.size(); ++i) {
    std::string_view sv{storage_.data() + offsets_[i], lengths_[i]};
    map_.emplace(sv, i);
  }
}

std::uint32_t StringPool::intern(std::string_view s) {
  auto it = map_.find(s);
  if (it != map_.end()) return it->second;
  auto id = static_cast<std::uint32_t>(offsets_.size());
  const char* old_data = storage_.empty() ? nullptr : storage_.data();
  auto off = static_cast<std::uint32_t>(storage_.size());
  storage_.insert(storage_.end(), s.begin(), s.end());
  offsets_.push_back(off);
  lengths_.push_back(static_cast<std::uint32_t>(s.size()));
  // string_view keys alias storage_; rebuild if a reallocation invalidated them.
  if (old_data && storage_.data() != old_data) {
    rebuild_map();
  } else {
    map_.emplace(std::string_view{storage_.data() + off, s.size()}, id);
  }
  return id;
}

std::string_view StringPool::get(std::uint32_t id) const {
  if (id >= offsets_.size()) return {};
  return {storage_.data() + offsets_[id], lengths_[id]};
}

std::uint32_t StringPool::find(std::string_view s) const {
  auto it = map_.find(s);
  if (it == map_.end()) return kInvalid;
  return it->second;
}

void StringPool::serialize(std::vector<std::uint8_t>& out) const {
  auto push32 = [&](std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 24));
  };
  push32(static_cast<std::uint32_t>(offsets_.size()));
  push32(static_cast<std::uint32_t>(storage_.size()));
  for (std::uint32_t i = 0; i < offsets_.size(); ++i) push32(lengths_[i]);
  out.insert(out.end(), storage_.begin(), storage_.end());
}

bool StringPool::deserialize(const std::uint8_t* data, std::size_t size) {
  auto rd32 = [&](std::size_t off) -> std::uint32_t {
    return static_cast<std::uint32_t>(data[off]) |
           (static_cast<std::uint32_t>(data[off + 1]) << 8) |
           (static_cast<std::uint32_t>(data[off + 2]) << 16) |
           (static_cast<std::uint32_t>(data[off + 3]) << 24);
  };
  if (size < 8) return false;
  std::uint32_t n = rd32(0);
  std::uint32_t bytes = rd32(4);
  std::size_t need = 8ull + 4ull * n + bytes;
  if (size < need) return false;
  clear();
  offsets_.resize(n);
  lengths_.resize(n);
  std::size_t cursor = 8;
  std::uint32_t off = 0;
  for (std::uint32_t i = 0; i < n; ++i) {
    lengths_[i] = rd32(cursor);
    offsets_[i] = off;
    off += lengths_[i];
    cursor += 4;
  }
  storage_.assign(reinterpret_cast<const char*>(data + cursor),
                  reinterpret_cast<const char*>(data + cursor + bytes));
  if (off != bytes) return false;
  rebuild_map();
  return true;
}

void StringPool::clear() {
  storage_.clear();
  offsets_.clear();
  lengths_.clear();
  map_.clear();
}

}  // namespace wilfred
