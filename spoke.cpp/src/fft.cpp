#include "spoke/fft.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace spoke {

bool Fft::isPowerOfTwo(size_t n) { return n && !(n & (n - 1)); }

size_t Fft::nextPowerOfTwo(size_t n) {
  if (n == 0) return 1;
  size_t p = 1;
  while (p < n) p <<= 1;
  return p;
}

Fft::Fft(size_t n) : n_(n) {
  // Round up to power of two for safety.
  if (!isPowerOfTwo(n_)) n_ = nextPowerOfTwo(n_);
  log2n_ = 0;
  for (size_t t = n_; t > 1; t >>= 1) ++log2n_;
  rev_.resize(n_);
  for (size_t i = 0; i < n_; ++i) {
    size_t r = 0;
    for (size_t b = 0; b < log2n_; ++b)
      if (i & (size_t(1) << b)) r |= size_t(1) << (log2n_ - 1 - b);
    rev_[i] = r;
  }
  cos_table_.resize(n_ / 2);
  sin_table_.resize(n_ / 2);
  for (size_t i = 0; i < n_ / 2; ++i) {
    double ang = -2.0 * M_PI * i / n_;
    cos_table_[i] = static_cast<float>(std::cos(ang));
    sin_table_[i] = static_cast<float>(std::sin(ang));
  }
}

void Fft::forward(float* real, float* imag) const {
  const size_t n = n_;
  // Bit-reversal permutation.
  for (size_t i = 0; i < n; ++i) {
    size_t j = rev_[i];
    if (j > i) {
      float tr = real[i];
      real[i] = real[j];
      real[j] = tr;
      float ti = imag[i];
      imag[i] = imag[j];
      imag[j] = ti;
    }
  }
  // Iterative butterflies.
  for (size_t len = 2; len <= n; len <<= 1) {
    size_t step = n_ / len;
    size_t half = len / 2;
    for (size_t base = 0; base < n; base += len) {
      for (size_t j = 0; j < half; ++j) {
        float c = cos_table_[j * step];
        float s = sin_table_[j * step];
        float ur = real[base + j];
        float ui = imag[base + j];
        float vr = real[base + j + half];
        float vi = imag[base + j + half];
        float tr = vr * c - vi * s;
        float ti = vr * s + vi * c;
        real[base + j] = ur + tr;
        imag[base + j] = ui + ti;
        real[base + j + half] = ur - tr;
        imag[base + j + half] = ui - ti;
      }
    }
  }
}

void Fft::powerSpectrum(const float* input, float* power_out) const {
  const size_t n = n_;
  // Copy into temp buffers (caller guarantees power_out has n/2+1).
  // Use static thread_local scratch to avoid allocs in hot loop.
  thread_local std::vector<float> r, im;
  if (r.size() < n) {
    r.resize(n);
    im.resize(n);
  }
  for (size_t i = 0; i < n; ++i) {
    r[i] = input[i];
    im[i] = 0.0f;
  }
  forward(r.data(), im.data());
  float inv_n = 1.0f / static_cast<float>(n);
  for (size_t k = 0; k <= n / 2; ++k)
    power_out[k] = (r[k] * r[k] + im[k] * im[k]) * inv_n;
}

std::vector<float> hannWindow(size_t n) {
  std::vector<float> w(n);
  for (size_t i = 0; i < n; ++i)
    w[i] = 0.5f * (1.0f - static_cast<float>(std::cos(2.0 * M_PI * i / (n - 1))));
  return w;
}

}  // namespace spoke
