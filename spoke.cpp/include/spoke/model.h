#pragma once
#include <cstddef>
#include <string>
#include <vector>
#include "spoke/config.h"
#include "spoke/quant.h"
#include "spoke/tensor.h"

namespace spoke {

// Forward declarations for threading (avoid heavy include in header).
class ThreadPool;

/// Single affine layer with optional int8 fast path.
struct Linear {
  size_t in_dim = 0, out_dim = 0;
  std::vector<float> weight;  // [out,in] row-major
  std::vector<float> bias;    // [out]
  QuantMatrix qweight;        // quantized mirror
  bool quantized = false;

  Linear() = default;
  Linear(size_t in, size_t out);
  Tensor forward(const Tensor& x) const;              // x: [T,in] -> [T,out]
  std::vector<float> forwardFrame(const float* x) const;
  void quantize();  // build int8 mirror
};

/// Depthwise-separable 1D conv block: DW(k=5,pad=2) -> LN -> ReLU -> PW.
/// Residual passthrough when dims match. Tiny and cache-friendly.
struct ConvBlock {
  size_t dim = 0;
  int kernel = 5;
  std::vector<float> dw_weight;  // [dim x kernel]
  std::vector<float> dw_bias;    // [dim]
  Linear pw1;                    // dim -> 2*dim (expansion)
  Linear pw2;                    // 2*dim -> dim (projection)

  ConvBlock() = default;
  ConvBlock(size_t dim_, unsigned seed);
  // seq: [T x dim] in/out (in-place friendly, returns new).
  std::vector<std::vector<float>> forward(const std::vector<std::vector<float>>& seq) const;
};

/// GRU layer over time (batch=1). Compact, no cuDNN needed.
struct GruLayer {
  size_t input_dim = 0, hidden_dim = 0;
  Linear wz, wr, wh;  // input projections
  Linear uz, ur, uh;  // hidden projections (no bias for U)

  GruLayer() = default;
  GruLayer(size_t in, size_t hidden, unsigned seed);
  std::vector<std::vector<float>> forward(const std::vector<std::vector<float>>& seq) const;
};

/// TinyEncoder: input proj (mel->dim) + N conv blocks + GRU + LN.
/// Deterministic Xavier init with fixed seed => reproducible, no blob needed.
class TinyEncoder {
 public:
  explicit TinyEncoder(const SpokeConfig& cfg, unsigned seed = 42);

  // mel: [T x n_mels] -> hidden: [T' x dim] (T'==T; no striding in tiny).
  std::vector<std::vector<float>> forward(const std::vector<std::vector<float>>& mel,
                                          ThreadPool* pool = nullptr) const;
  size_t paramCount() const;
  int dim() const { return dim_; }

 private:
  SpokeConfig cfg_;
  int dim_ = 128;
  Linear input_proj_;
  std::vector<ConvBlock> blocks_;
  GruLayer gru_;
};

/// Full acoustic model: encoder + CTC projection to vocab (29 symbols).
class AcousticModel {
 public:
  explicit AcousticModel(const SpokeConfig& cfg, unsigned seed = 42);

  // Returns per-frame logits [T x vocab]. vocab=29 (0=blank).
  std::vector<std::vector<float>> forward(const std::vector<std::vector<float>>& mel,
                                          ThreadPool* pool = nullptr) const;
  size_t paramCount() const;
  int vocabSize() const { return 29; }

 private:
  SpokeConfig cfg_;
  TinyEncoder encoder_;
  Linear ctc_proj_;
};

std::vector<float> xavierInit(size_t n, unsigned seed, float gain = 1.0f);

}  // namespace spoke
