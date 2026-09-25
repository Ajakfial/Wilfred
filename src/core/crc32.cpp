#include "wilfred/core/crc32.hpp"

#include <mutex>

namespace wilfred {

static std::uint32_t table[256];
static std::once_flag table_ready;

static void init_table() {
  for (std::uint32_t i = 0; i < 256; ++i) {
    std::uint32_t c = i;
    for (int j = 0; j < 8; ++j) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
    table[i] = c;
  }
}

std::uint32_t crc32(const void* data, std::size_t size, std::uint32_t seed) {
  std::call_once(table_ready, init_table);
  std::uint32_t c = seed ^ 0xFFFFFFFFu;
  auto* p = static_cast<const std::uint8_t*>(data);
  for (std::size_t i = 0; i < size; ++i) c = table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
  return c ^ 0xFFFFFFFFu;
}

}  // namespace wilfred
