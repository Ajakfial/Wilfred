#include "wilfred/core/mmap.hpp"

#include "wilfred/core/utf8.hpp"

#include <fstream>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <filesystem>

namespace wilfred {
namespace fs = std::filesystem;

MappedFile::~MappedFile() { close(); }

MappedFile::MappedFile(MappedFile&& other) noexcept { *this = std::move(other); }

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
  if (this == &other) return *this;
  close();
  data_ = other.data_;
  size_ = other.size_;
#ifdef _WIN32
  file_ = other.file_;
  mapping_ = other.mapping_;
  other.file_ = nullptr;
  other.mapping_ = nullptr;
#else
  fd_ = other.fd_;
  other.fd_ = -1;
#endif
  other.data_ = nullptr;
  other.size_ = 0;
  return *this;
}

void MappedFile::close() {
#ifdef _WIN32
  if (data_) UnmapViewOfFile(data_);
  if (mapping_) CloseHandle(mapping_);
  if (file_) CloseHandle(file_);
  file_ = mapping_ = nullptr;
#else
  if (data_ && size_) munmap(const_cast<std::uint8_t*>(data_), size_);
  if (fd_ >= 0) ::close(fd_);
  fd_ = -1;
#endif
  data_ = nullptr;
  size_ = 0;
}

bool MappedFile::open_read(const std::string& path) {
  close();
#ifdef _WIN32
  auto w = utf8_to_wide(path);
  file_ = CreateFileW(w.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                      OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file_ == INVALID_HANDLE_VALUE) {
    file_ = nullptr;
    return false;
  }
  LARGE_INTEGER li{};
  if (!GetFileSizeEx(file_, &li) || li.QuadPart <= 0) {
    close();
    return false;
  }
  size_ = static_cast<std::size_t>(li.QuadPart);
  mapping_ = CreateFileMappingW(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
  if (!mapping_) {
    close();
    return false;
  }
  data_ = static_cast<const std::uint8_t*>(MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0));
  if (!data_) {
    close();
    return false;
  }
  return true;
#else
  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) return false;
  struct stat st {};
  if (fstat(fd_, &st) != 0 || st.st_size <= 0) {
    close();
    return false;
  }
  size_ = static_cast<std::size_t>(st.st_size);
  auto* p = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
  if (p == MAP_FAILED) {
    close();
    return false;
  }
  data_ = static_cast<const std::uint8_t*>(p);
  return true;
#endif
}

bool write_file_atomic(const std::string& path, const void* data, std::size_t size) {
  std::string tmp = path + ".tmp";
  {
#ifdef _WIN32
    auto w = utf8_to_wide(tmp);
    HANDLE h = CreateFileW(w.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    const char* p = static_cast<const char*>(data);
    std::size_t left = size;
    while (left) {
      DWORD chunk = left > 0x40000000u ? 0x40000000u : static_cast<DWORD>(left);
      DWORD wr = 0;
      if (!WriteFile(h, p, chunk, &wr, nullptr) || wr != chunk) {
        CloseHandle(h);
        return false;
      }
      p += wr;
      left -= wr;
    }
    FlushFileBuffers(h);
    CloseHandle(h);
    auto dest = utf8_to_wide(path);
    if (!MoveFileExW(w.c_str(), dest.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      return false;
    return true;
#else
    int fd = ::open(tmp.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) return false;
    const char* p = static_cast<const char*>(data);
    std::size_t left = size;
    while (left) {
      auto n = ::write(fd, p, left);
      if (n < 0) {
        ::close(fd);
        return false;
      }
      p += n;
      left -= static_cast<std::size_t>(n);
    }
    ::fsync(fd);
    ::close(fd);
    return std::rename(tmp.c_str(), path.c_str()) == 0;
#endif
  }
}

bool read_file_all(const std::string& path, std::string& out) {
  std::ifstream in(fs::u8path(path), std::ios::binary);
  if (!in) return false;
  in.seekg(0, std::ios::end);
  auto n = in.tellg();
  if (n < 0) return false;
  out.resize(static_cast<std::size_t>(n));
  in.seekg(0);
  in.read(out.data(), n);
  return static_cast<bool>(in);
}

bool file_exists(const std::string& path) {
  std::error_code ec;
  return fs::exists(fs::u8path(path), ec);
}

bool create_directories(const std::string& path) {
  std::error_code ec;
  fs::create_directories(fs::u8path(path), ec);
  return !ec;
}

std::uint64_t file_size_bytes(const std::string& path) {
  std::error_code ec;
  auto n = fs::file_size(fs::u8path(path), ec);
  return ec ? 0 : static_cast<std::uint64_t>(n);
}

bool remove_file(const std::string& path) {
  std::error_code ec;
  fs::remove(fs::u8path(path), ec);
  return !ec;
}

}  // namespace wilfred
