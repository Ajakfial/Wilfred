#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace spoke {

/// Symmetric int8 quantization for encoder matmuls.
/// One scale per row (per output channel) — accurate yet cheap.
struct QuantRowParams {
  float scale = 1.0f;  // dequant: float = int8 * scale
};

QuantRowParams chooseQuantParams(const float* row, size_t n);
void quantizeRowInt8(const float* row, int8_t* qout, size_t n, QuantRowParams& params);
void dequantizeRowInt8(const int8_t* q, float* out, size_t n, QuantRowParams params);

/// Quantized weight matrix: [rows x cols] int8 + per-row scales.
struct QuantMatrix {
  size_t rows = 0, cols = 0;
  std::vector<int8_t> data;
  std::vector<float> scales;
  static QuantMatrix quantize(const float* w, size_t rows, size_t cols);
  void matvec(const float* x, float* y, size_t rows, size_t cols) const;
};

/// Fast float dot with manual unrolling (auto-vectorized by MSVC/clang -O2).
float dotFloat(const float* a, const float* b, size_t n);

}  // namespace spoke
