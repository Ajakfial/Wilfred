#include "spoke/model.h"
#include <cmath>
#include <random>

namespace spoke {

std::vector<float> xavierInit(size_t n, unsigned seed, float gain) {
  std::mt19937 rng(seed);
  // Xavier uniform bound needs fan-in; approximate with sqrt(6/n) scaled.
  float bound = gain * std::sqrt(6.0f / static_cast<float>(n > 0 ? n : 1));
  std::uniform_real_distribution<float> dist(-bound, bound);
  std::vector<float> w(n);
  for (auto& v : w) v = dist(rng);
  return w;
}

// --- Linear -----------------------------------------------------------------

Linear::Linear(size_t in, size_t out) : in_dim(in), out_dim(out) {
  weight.assign(out * in, 0.0f);
  bias.assign(out, 0.0f);
}

Tensor Linear::forward(const Tensor& x) const {
  // x: [T,in]
  size_t T = x.dim(0);
  Tensor y({T, out_dim});
  for (size_t t = 0; t < T; ++t) {
    const float* xr = x.data() + t * in_dim;
    float* yr = y.data() + t * out_dim;
    if (quantized)
      qweight.matvec(xr, yr, out_dim, in_dim);
    else
      matvec(weight.data(), xr, bias.data(), yr, out_dim, in_dim);
    if (quantized) addInplace(yr, bias.data(), out_dim);
  }
  return y;
}

std::vector<float> Linear::forwardFrame(const float* x) const {
  std::vector<float> y(out_dim);
  if (quantized) {
    qweight.matvec(x, y.data(), out_dim, in_dim);
    addInplace(y.data(), bias.data(), out_dim);
  } else {
    matvec(weight.data(), x, bias.data(), y.data(), out_dim, in_dim);
  }
  return y;
}

void Linear::quantize() {
  qweight = QuantMatrix::quantize(weight.data(), out_dim, in_dim);
  quantized = true;
}

// --- ConvBlock ---------------------------------------------------------------

ConvBlock::ConvBlock(size_t dim_, unsigned seed) : dim(dim_), pw1(dim_, dim_ * 2), pw2(dim_ * 2, dim_) {
  dw_weight = xavierInit(dim * kernel, seed, 1.0f);
  dw_bias.assign(dim, 0.0f);
  pw1.weight = xavierInit(dim * 2 * dim, seed + 1);
  pw1.bias.assign(dim * 2, 0.0f);
  pw2.weight = xavierInit(dim * dim * 2, seed + 2);
  pw2.bias.assign(dim, 0.0f);
}

std::vector<std::vector<float>> ConvBlock::forward(
    const std::vector<std::vector<float>>& seq) const {
  size_t T = seq.size();
  if (T == 0) return {};
  size_t D = dim;
  int K = kernel, pad = K / 2;
  // Depthwise conv.
  std::vector<std::vector<float>> dw(T, std::vector<float>(D));
  for (size_t t = 0; t < T; ++t) {
    for (size_t d = 0; d < D; ++d) {
      float acc = dw_bias[d];
      for (int k = 0; k < K; ++k) {
        long tt = static_cast<long>(t) + k - pad;
        float v = 0.0f;
        if (tt >= 0 && tt < static_cast<long>(T)) v = seq[static_cast<size_t>(tt)][d];
        acc += dw_weight[d * K + k] * v;
      }
      dw[t][d] = acc;
    }
  }
  // Residual + LN + expansion/projection (position-wise FFN).
  std::vector<std::vector<float>> out(T, std::vector<float>(D));
  std::vector<float> tmp1(D * 2), tmp2(D), normed(D);
  for (size_t t = 0; t < T; ++t) {
    // residual add
    for (size_t d = 0; d < D; ++d) dw[t][d] += seq[t][d];
    layerNorm(dw[t].data(), normed.data(), D);
    // pw1 + relu
    matvec(pw1.weight.data(), normed.data(), pw1.bias.data(), tmp1.data(), D * 2, D);
    reluInplace(tmp1.data(), D * 2);
    // pw2
    matvec(pw2.weight.data(), tmp1.data(), pw2.bias.data(), tmp2.data(), D, D * 2);
    // second residual
    for (size_t d = 0; d < D; ++d) out[t][d] = normed[d] + tmp2[d];
  }
  return out;
}

// --- GRU ---------------------------------------------------------------------

GruLayer::GruLayer(size_t in, size_t hidden, unsigned seed)
    : input_dim(in),
      hidden_dim(hidden),
      wz(in, hidden),
      wr(in, hidden),
      wh(in, hidden),
      uz(hidden, hidden),
      ur(hidden, hidden),
      uh(hidden, hidden) {
  wz.weight = xavierInit(in * hidden, seed + 11, 1.0f);
  wr.weight = xavierInit(in * hidden, seed + 12, 1.0f);
  wh.weight = xavierInit(in * hidden, seed + 13, 1.0f);
  uz.weight = xavierInit(hidden * hidden, seed + 14, 1.0f);
  ur.weight = xavierInit(hidden * hidden, seed + 15, 1.0f);
  uh.weight = xavierInit(hidden * hidden, seed + 16, 1.0f);
  wz.bias.assign(hidden, 0.0f);
  wr.bias.assign(hidden, 0.0f);
  wh.bias.assign(hidden, 0.0f);
}

std::vector<std::vector<float>> GruLayer::forward(
    const std::vector<std::vector<float>>& seq) const {
  size_t T = seq.size(), H = hidden_dim;
  std::vector<std::vector<float>> out;
  out.reserve(T);
  std::vector<float> h(H, 0.0f), z(H), r(H), n(H), tmp(H);
  for (size_t t = 0; t < T; ++t) {
    const float* x = seq[t].data();
    // z = sigm(Wz x + Uz h)
    auto wzx = wz.forwardFrame(x);
    matvec(uz.weight.data(), h.data(), nullptr, tmp.data(), H, H);
    for (size_t i = 0; i < H; ++i) z[i] = 1.0f / (1.0f + std::exp(-(wzx[i] + tmp[i])));
    // r = sigm(Wr x + Ur h)
    auto wrx = wr.forwardFrame(x);
    matvec(ur.weight.data(), h.data(), nullptr, tmp.data(), H, H);
    for (size_t i = 0; i < H; ++i) r[i] = 1.0f / (1.0f + std::exp(-(wrx[i] + tmp[i])));
    // n = tanh(Wh x + Uh (r*h))
    auto whx = wh.forwardFrame(x);
    for (size_t i = 0; i < H; ++i) tmp[i] = r[i] * h[i];
    std::vector<float> uhr(H);
    matvec(uh.weight.data(), tmp.data(), nullptr, uhr.data(), H, H);
    for (size_t i = 0; i < H; ++i) n[i] = std::tanh(whx[i] + uhr[i]);
    for (size_t i = 0; i < H; ++i) h[i] = (1 - z[i]) * n[i] + z[i] * h[i];
    out.push_back(h);
  }
  return out;
}

// --- TinyEncoder --------------------------------------------------------------

TinyEncoder::TinyEncoder(const SpokeConfig& cfg, unsigned seed) : cfg_(cfg), dim_(cfg.encoder_dim) {
  input_proj_ = Linear(static_cast<size_t>(cfg.n_mels), static_cast<size_t>(dim_));
  input_proj_.weight = xavierInit(static_cast<size_t>(cfg.n_mels) * dim_, seed + 100);
  input_proj_.bias.assign(dim_, 0.0f);
  for (int i = 0; i < cfg.encoder_layers; ++i) blocks_.emplace_back(dim_, seed + 200 + i * 10);
  gru_ = GruLayer(dim_, dim_, seed + 500);
  if (cfg.use_int8) input_proj_.quantize();
}

std::vector<std::vector<float>> TinyEncoder::forward(
    const std::vector<std::vector<float>>& mel, ThreadPool* /*pool*/) const {
  size_t T = mel.size();
  if (T == 0) return {};
  // Input projection.
  std::vector<std::vector<float>> h(T, std::vector<float>(dim_));
  for (size_t t = 0; t < T; ++t) {
    auto y = input_proj_.forwardFrame(mel[t].data());
    reluInplace(y.data(), dim_);
    h[t] = std::move(y);
  }
  for (auto& b : blocks_) h = b.forward(h);
  h = gru_.forward(h);
  // Final LN.
  for (auto& fr : h) {
    std::vector<float> n(dim_);
    layerNorm(fr.data(), n.data(), dim_);
    fr = std::move(n);
  }
  return h;
}

size_t TinyEncoder::paramCount() const {
  size_t n = input_proj_.weight.size() + input_proj_.bias.size();
  for (auto& b : blocks_)
    n += b.dw_weight.size() + b.dw_bias.size() + b.pw1.weight.size() + b.pw1.bias.size() +
         b.pw2.weight.size() + b.pw2.bias.size();
  n += gru_.wz.weight.size() + gru_.wr.weight.size() + gru_.wh.weight.size() +
       gru_.uz.weight.size() + gru_.ur.weight.size() + gru_.uh.weight.size() + 3 * dim_;
  return n;
}

// --- AcousticModel ------------------------------------------------------------

AcousticModel::AcousticModel(const SpokeConfig& cfg, unsigned seed)
    : cfg_(cfg), encoder_(cfg, seed), ctc_proj_(encoder_.dim(), 29) {
  ctc_proj_.weight = xavierInit(encoder_.dim() * 29, seed + 900, 0.5f);
  ctc_proj_.bias.assign(29, 0.0f);
  // Slight blank bias so silence decodes to blank (no hallucination).
  ctc_proj_.bias[0] = 1.0f;
  if (cfg.use_int8) ctc_proj_.quantize();
}

std::vector<std::vector<float>> AcousticModel::forward(
    const std::vector<std::vector<float>>& mel, ThreadPool* pool) const {
  auto h = encoder_.forward(mel, pool);
  std::vector<std::vector<float>> logits;
  logits.reserve(h.size());
  for (auto& fr : h) logits.push_back(ctc_proj_.forwardFrame(fr.data()));
  return logits;
}

size_t AcousticModel::paramCount() const {
  return encoder_.paramCount() + ctc_proj_.weight.size() + ctc_proj_.bias.size();
}

}  // namespace spoke
