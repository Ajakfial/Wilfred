#pragma once
#include <string>
#include <vector>
#include "spoke/tokenizer.h"

namespace spoke {

std::vector<float> logSoftmax(const std::vector<float>& logits);

/// Greedy CTC: argmax per frame, collapse repeats, drop blanks.
std::string ctcGreedyDecode(const std::vector<std::vector<float>>& log_probs,
                            const EnglishTokenizer& tok);

/// Greedy CTC returning per-frame ids (collapsed) + confidence.
struct GreedyResult {
  std::string text;
  float confidence = 0.0f;  // mean max-prob over emitted frames
};

GreedyResult ctcGreedyDecodeDetailed(const std::vector<std::vector<float>>& log_probs,
                                     const EnglishTokenizer& tok);

struct BeamSearchConfig {
  int beam_width = 8;
  float lm_weight = 0.15f;  // lexicon bonus for known words
};

struct Hypothesis {
  std::string text;
  float score = 0.0f;
};

/// Prefix beam search over CTC log-probs with tiny lexicon shallow fusion.
/// Lexicon is a small built-in English word list (no external LM file).
std::vector<Hypothesis> ctcBeamSearch(const std::vector<std::vector<float>>& log_probs,
                                      const EnglishTokenizer& tok,
                                      const BeamSearchConfig& cfg);

/// Word-level confidence from frame posteriors (mean of word-span peaks).
float wordConfidence(const std::vector<std::vector<float>>& log_probs,
                     const EnglishTokenizer& tok);

}  // namespace spoke
