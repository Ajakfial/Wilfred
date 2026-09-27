#pragma once

#include <cstddef>
#include <type_traits>

namespace wilfred {

template <typename T>
class Span {
public:
  Span() = default;
  Span(T* data, std::size_t size) : data_(data), size_(size) {}
  template <typename C>
  explicit Span(C& c) : data_(c.data()), size_(c.size()) {}

  T* data() const { return data_; }
  std::size_t size() const { return size_; }
  bool empty() const { return size_ == 0; }
  T& operator[](std::size_t i) const { return data_[i]; }
  T* begin() const { return data_; }
  T* end() const { return data_ + size_; }

private:
  T* data_{nullptr};
  std::size_t size_{0};
};

}  // namespace wilfred
