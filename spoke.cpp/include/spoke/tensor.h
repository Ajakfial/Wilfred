#pragma once
#include <cstddef>
#include <vector>

namespace spoke {

/// Minimal row-major dense tensor used by the acoustic model.
/// Kept deliberately small: no broadcasting, no autograd, cache-friendly.
class Tensor {
 public:
  Tensor() = default;
  Tensor(std::vector<size_t> shape);
  Tensor(std::vector<size_t> shape, std::vector<float> data);

  static Tensor zeros(std::vector<size_t> shape);
  static Tensor fromVector(const std::vector<float>& v);

  const std::vector<size_t>& shape() const { return shape_; }
  size_t rank() const { return shape_.size(); }
  size_t numel() const { return data_.size(); }
  size_t dim(size_t i) const { return shape_.at(i); }

  float* data() { return data_.data(); }
  const float* data() const { return data_.data(); }
  std::vector<float>& raw() { return data_; }
  const std::vector<float>& raw() const { return data_; }

  float& at(size_t r, size_t c) { return data_[r * shape_[1] + c]; }
  float at(size_t r, size_t c) const { return data_[r * shape_[1] + c]; }

  void fill(float v);
  void reshape(std::vector<size_t> new_shape);

 private:
  std::vector<size_t> shape_;
  std::vector<float> data_;
};

/// y = W x + b, W is [out,in]. Cache-blocked, thread-friendly.
void matvec(const float* W, const float* x, const float* b, float* y, size_t out_dim,
            size_t in_dim);

/// C = A @ B for small matrices, row-major.
void matmulBlocked(const float* A, const float* B, float* C, size_t M, size_t K, size_t N);

void addInplace(float* a, const float* b, size_t n);
void reluInplace(float* a, size_t n);
void tanhInplace(float* a, size_t n);
void sigmoidInplace(float* a, size_t n);
void layerNorm(const float* x, float* y, size_t n, float eps = 1e-5f);

}  // namespace spoke
