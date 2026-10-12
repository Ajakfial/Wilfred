#include "spoke/tensor.h"
#include <cmath>
#include <stdexcept>

namespace spoke {

Tensor::Tensor(std::vector<size_t> shape) : shape_(std::move(shape)) {
  size_t n = 1;
  for (size_t d : shape_) n *= d;
  data_.assign(n, 0.0f);
}

Tensor::Tensor(std::vector<size_t> shape, std::vector<float> data)
    : shape_(std::move(shape)), data_(std::move(data)) {}

Tensor Tensor::zeros(std::vector<size_t> shape) { return Tensor(std::move(shape)); }

Tensor Tensor::fromVector(const std::vector<float>& v) {
  return Tensor({v.size()}, v);
}

void Tensor::fill(float v) { std::fill(data_.begin(), data_.end(), v); }

void Tensor::reshape(std::vector<size_t> new_shape) {
  size_t n = 1;
  for (size_t d : new_shape) n *= d;
  if (n != data_.size()) throw std::runtime_error("reshape: size mismatch");
  shape_ = std::move(new_shape);
}

void matvec(const float* W, const float* x, const float* b, float* y, size_t out_dim,
            size_t in_dim) {
  for (size_t o = 0; o < out_dim; ++o) {
    const float* wrow = W + o * in_dim;
    float acc = b ? b[o] : 0.0f;
    // Manual 4x unroll helps MSVC auto-vectorize.
    size_t i = 0;
    float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
    for (; i + 4 <= in_dim; i += 4) {
      s0 += wrow[i] * x[i];
      s1 += wrow[i + 1] * x[i + 1];
      s2 += wrow[i + 2] * x[i + 2];
      s3 += wrow[i + 3] * x[i + 3];
    }
    acc += s0 + s1 + s2 + s3;
    for (; i < in_dim; ++i) acc += wrow[i] * x[i];
    y[o] = acc;
  }
}

void matmulBlocked(const float* A, const float* B, float* C, size_t M, size_t K, size_t N) {
  constexpr size_t BM = 32;
  for (size_t i = 0; i < M * N; ++i) C[i] = 0.0f;
  for (size_t i0 = 0; i0 < M; i0 += BM) {
    for (size_t k0 = 0; k0 < K; k0 += BM) {
      for (size_t j0 = 0; j0 < N; j0 += BM) {
        size_t i1 = std::min(i0 + BM, M), k1 = std::min(k0 + BM, K),
               j1 = std::min(j0 + BM, N);
        for (size_t i = i0; i < i1; ++i)
          for (size_t k = k0; k < k1; ++k) {
            float a = A[i * K + k];
            for (size_t j = j0; j < j1; ++j) C[i * N + j] += a * B[k * N + j];
          }
      }
    }
  }
}

void addInplace(float* a, const float* b, size_t n) {
  for (size_t i = 0; i < n; ++i) a[i] += b[i];
}
void reluInplace(float* a, size_t n) {
  for (size_t i = 0; i < n; ++i) a[i] = a[i] > 0 ? a[i] : 0;
}
void tanhInplace(float* a, size_t n) {
  for (size_t i = 0; i < n; ++i) a[i] = std::tanh(a[i]);
}
void sigmoidInplace(float* a, size_t n) {
  for (size_t i = 0; i < n; ++i) a[i] = 1.0f / (1.0f + std::exp(-a[i]));
}
void layerNorm(const float* x, float* y, size_t n, float eps) {
  double mean = 0;
  for (size_t i = 0; i < n; ++i) mean += x[i];
  mean /= n;
  double var = 0;
  for (size_t i = 0; i < n; ++i) {
    double d = x[i] - mean;
    var += d * d;
  }
  var /= n;
  float inv = 1.0f / std::sqrt(static_cast<float>(var) + eps);
  for (size_t i = 0; i < n; ++i) y[i] = static_cast<float>((x[i] - mean) * inv);
}

}  // namespace spoke
