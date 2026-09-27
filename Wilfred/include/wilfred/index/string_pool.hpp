#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace wilfred {

class StringPool {
public:
  static constexpr std::uint32_t kInvalid = 0xFFFFFFFFu;

  std::uint32_t intern(std::string_view s);
  std::string_view get(std::uint32_t id) const;
  std::uint32_t find(std::string_view s) const;
  std::size_t count() const { return offsets_.size(); }
  std::size_t bytes() const { return storage_.size(); }

  void serialize(std::vector<std::uint8_t>& out) const;
  bool deserialize(const std::uint8_t* data, std::size_t size);

  void clear();

private:
  std::vector<char> storage_;
  std::vector<std::uint32_t> offsets_;
  std::vector<std::uint32_t> lengths_;
  std::unordered_map<std::string_view, std::uint32_t> map_;

  void rebuild_map();
};

}  // namespace wilfred
