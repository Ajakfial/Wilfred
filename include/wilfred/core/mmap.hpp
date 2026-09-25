#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace wilfred {

class MappedFile {
public:
  MappedFile() = default;
  ~MappedFile();
  MappedFile(MappedFile&& other) noexcept;
  MappedFile& operator=(MappedFile&& other) noexcept;
  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;

  bool open_read(const std::string& path);
  void close();

  const std::uint8_t* data() const { return data_; }
  std::size_t size() const { return size_; }
  std::string_view view() const {
    return {reinterpret_cast<const char*>(data_), size_};
  }
  bool valid() const { return data_ != nullptr; }

private:
  const std::uint8_t* data_{nullptr};
  std::size_t size_{0};
#ifdef _WIN32
  void* file_{nullptr};
  void* mapping_{nullptr};
#else
  int fd_{-1};
#endif
};

bool write_file_atomic(const std::string& path, const void* data, std::size_t size);
bool read_file_all(const std::string& path, std::string& out);
bool file_exists(const std::string& path);
bool create_directories(const std::string& path);
std::uint64_t file_size_bytes(const std::string& path);
bool remove_file(const std::string& path);

}  // namespace wilfred
