#pragma once
#include <cstddef>
#include <vector>

namespace spoke {

/// Radix-2 iterative Cooley-Tukey FFT, from scratch (no FFTW / no KissFFT).
/// Only power-of-two sizes are supported; n_fft=512 is used internally
/// (400-sample frames are zero-padded to 512).
class Fft {
 public:
  explicit Fft(size_t n);
  size_t size() const { return n_; }

  /// In-place complex forward FFT: real[i] + j*imag[i].
  void forward(float* real, float* imag) const;

  /// Real-input convenience: computes power spectrum |X|^2 / n.
  void powerSpectrum(const float* input, float* power_out) const;

  static bool isPowerOfTwo(size_t n);
  static size_t nextPowerOfTwo(size_t n);

 private:
  size_t n_;
  size_t log2n_;
  std::vector<size_t> rev_;          // bit-reversed indices
  std::vector<float> cos_table_;     // twiddle cos
  std::vector<float> sin_table_;     // twiddle sin
};

std::vector<float> hannWindow(size_t n);

}  // namespace spoke
