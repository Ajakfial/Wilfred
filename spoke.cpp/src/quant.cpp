#include "spoke/quant.h"
#include <algorithm>
#include <cmath>

namespace spoke {

QuantRowParams chooseQuantParams(const float* row, size_t n) {
  float m = 0;
  for (size_t i = 0; i < n; ++i) m = std::max(m, std::fabs(row[i]));
  QuantRowParams p;
  p.scale = (m < 1e-8f) ? 1.0f : m / 127.0f;
  return p;
}

void quantizeRowInt8(const float* row, int8_t* qout, size_t n, QuantRowParams& params) {
  params = chooseQuantParams(row, n);
  float inv = 1.0f / params.scale;
  for (size_t i = 0; i < n; ++i) {
    int v = static_cast<int>(std::lround(row[i] * inv));
    v = std::max(-127, std::min(127, v));
    qout[i] = static_cast<int8_t>(v);
  }
}

void dequantizeRowInt8(const int8_t* q, float* out, size_t n, QuantRowParams params) {
  for (size_t i = 0; i < n; ++i) out[i] = q[i] * params.scale;
}

QuantMatrix QuantMatrix::quantize(const float* w, size_t rows, size_t cols) {
  QuantMatrix q;
  q.rows = rows;
  q.cols = cols;
  q.data.resize(rows * cols);
  q.scales.resize(rows);
  for (size_t r = 0; r < rows; ++r) {
    QuantRowParams p;
    quantizeRowInt8(w + r * cols, q.data.data() + r * cols, cols, p);
    q.scales[r] = p.scale;
  }
  return q;
}

void QuantMatrix::matvec(const float* x, float* y, size_t rows, size_t cols) const {
  for (size_t r = 0; r < rows; ++r) {
    const int8_t* wrow = data.data() + r * cols;
    int acc_round = 0;
    // Accumulate in integer domain, scale once at the end (key optimization).
    int total = 0;
    // Unrolled int accumulation; x is float so we scale: use float acc with
    // int8 weights -- still 2x less memory traffic than float weights.
    float acc = 0.0f;
    size_t i = 0;
    for (; i + 4 <= cols; i += 4) {
      acc += wrow[i] * x[i] + wrow[i + 1] * x[i + 1] + wrow[i + 2] * x[i + 2] +
             wrow[i + 3] * x[i + 3];
    }
    for (; i < cols; ++i) acc += wrow[i] * x[i];
    (void)acc_round;
    (void)total;
    y[r] = acc * scales[r];
  }
}

float dotFloat(const float* a, const float* b, size_t n) {
  float s0 = 0, s1 = 0, s2 = 0, s3 = 0;
  size_t i = 0;
  for (; i + 4 <= n; i += 4) {
    s0 += a[i] * b[i];
    s1 += a[i + 1] * b[i + 1];
    s2 += a[i + 2] * b[i + 2];
    s3 += a[i + 3] * b[i + 3];
  }
  float s = s0 + s1 + s2 + s3;
  for (; i < n; ++i) s += a[i] * b[i];
  return s;
}

}  // namespace spoke
