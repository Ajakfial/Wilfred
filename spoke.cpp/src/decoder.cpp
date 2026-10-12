#include "spoke/decoder.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <sstream>

namespace spoke {

std::vector<float> logSoftmax(const std::vector<float>& logits) {
  float m = *std::max_element(logits.begin(), logits.end());
  double acc = 0;
  for (float v : logits) acc += std::exp(v - m);
  float lse = m + std::log(static_cast<float>(acc));
  std::vector<float> out(logits.size());
  for (size_t i = 0; i < logits.size(); ++i) out[i] = logits[i] - lse;
  return out;
}

std::string ctcGreedyDecode(const std::vector<std::vector<float>>& log_probs,
                            const EnglishTokenizer& tok) {
  return ctcGreedyDecodeDetailed(log_probs, tok).text;
}

GreedyResult ctcGreedyDecodeDetailed(const std::vector<std::vector<float>>& log_probs,
                                     const EnglishTokenizer& tok) {
  GreedyResult r;
  if (log_probs.empty()) return r;
  std::vector<int> collapsed;
  collapsed.reserve(log_probs.size());
  double conf_acc = 0;
  int conf_n = 0;
  int prev = -1;
  for (auto& lp : log_probs) {
    int best = 0;
    float bv = lp[0];
    for (size_t i = 1; i < lp.size(); ++i)
      if (lp[i] > bv) {
        bv = lp[i];
        best = static_cast<int>(i);
      }
    float prob = std::exp(bv);  // lp assumed log-probs; if logits, still monotonic
    if (best != EnglishTokenizer::kBlank && best != prev) {
      collapsed.push_back(best);
      conf_acc += prob;
      ++conf_n;
    }
    prev = best;
  }
  r.text = tok.decode(collapsed);
  r.confidence = conf_n ? static_cast<float>(conf_acc / conf_n) : 0.0f;
  return r;
}

// Tiny built-in lexicon for shallow fusion (common English words).
static const char* kLexicon[] = {
    "the", "be", "to",   "of",  "and", "a",   "in",  "that", "have", "i",
    "it",  "for", "not", "on",  "with", "he",  "as",  "you", "do",   "at",
    "this", "but", "his", "by",  "from", "they", "we", "say", "her",  "she",
    "or",  "an",  "will", "my",  "one",  "all", "would", "there", "their",
    "what", "so", "up",  "out",  "if",  "about", "who", "get", "which", "go",
    "me",  "hello", "world", "speech", "text", "audio", "transcribe", "test",
    "spoke", "model", "english", "quick", "brown", "fox", "time", "people",
};
static constexpr size_t kLexiconSize = sizeof(kLexicon) / sizeof(kLexicon[0]);

static bool isKnownWord(const std::string& w) {
  for (size_t i = 0; i < kLexiconSize; ++i)
    if (w == kLexicon[i]) return true;
  return false;
}

std::vector<Hypothesis> ctcBeamSearch(const std::vector<std::vector<float>>& log_probs,
                                      const EnglishTokenizer& tok,
                                      const BeamSearchConfig& cfg) {
  struct Beam {
    std::string text;
    double p_blank = -1e30, p_non_blank = -1e30;
    int last_id = -1;
  };
  auto logAdd = [](double a, double b) {
    if (a < -1e29) return b;
    if (b < -1e29) return a;
    if (a < b) std::swap(a, b);
    return a + std::log1p(std::exp(b - a));
  };
  std::vector<Beam> beams{{"", 0.0, -1e30, -1}};
  int W = std::max(1, cfg.beam_width);
  for (auto& lp : log_probs) {
    std::map<std::string, Beam> next;
    auto get = [&](const std::string& key) -> Beam& {
      auto it = next.find(key);
      if (it == next.end()) {
        Beam nb;
        nb.text = key;
        nb.p_blank = -1e30;
        nb.p_non_blank = -1e30;
        auto res = next.emplace(key, nb);
        return res.first->second;
      }
      return it->second;
    };
    for (auto& b : beams) {
      double pb = b.p_blank, pnb = b.p_non_blank;
      double total = logAdd(pb, pnb);
      // Blank transition: prefix unchanged, ending in blank.
      {
        Beam& nb = get(b.text);
        nb.p_blank = logAdd(nb.p_blank, total + lp[EnglishTokenizer::kBlank]);
      }
      for (size_t c = 0; c < lp.size(); ++c) {
        if (static_cast<int>(c) == EnglishTokenizer::kBlank) continue;
        double pc = lp[c];
        char ch = tok.idToChar(static_cast<int>(c));
        bool repeat = !b.text.empty() && b.text.back() == ch;
        if (repeat) {
          // Repeat without intervening blank: stay at same prefix.
          Beam& nb = get(b.text);
          nb.p_non_blank = logAdd(nb.p_non_blank, pnb + pc);
          // Repeat with blank in between would have produced a new char,
          // but char-level prefixes merge ("h"+"h" -> "hh" distinct from
          // "h"); keep the genuinely extended hypothesis from blank paths.
          std::string ext = b.text + ch;
          Beam& ne = get(ext);
          ne.p_non_blank = logAdd(ne.p_non_blank, pb + pc);
        } else {
          std::string nc = b.text + ch;
          Beam& nb = get(nc);
          nb.p_non_blank = logAdd(nb.p_non_blank, total + pc);
        }
      }
    }
    // Prune to beam width by total score.
    std::vector<Beam> cand;
    cand.reserve(next.size());
    for (auto& kv : next) cand.push_back(kv.second);
    std::sort(cand.begin(), cand.end(), [&](const Beam& a, const Beam& b2) {
      return logAdd(a.p_blank, a.p_non_blank) > logAdd(b2.p_blank, b2.p_non_blank);
    });
    if (cand.size() > static_cast<size_t>(W)) cand.resize(W);
    beams = std::move(cand);
  }
  std::vector<Hypothesis> out;
  for (auto& b : beams) {
    Hypothesis h;
    h.text = b.text;
    h.score = static_cast<float>(logAdd(b.p_blank, b.p_non_blank));
    // Lexicon bonus: +lm_weight per known word.
    std::istringstream iss(h.text);
    std::string w;
    int known = 0, total = 0;
    while (iss >> w) {
      ++total;
      if (isKnownWord(w)) ++known;
    }
    h.score += cfg.lm_weight * known;
    out.push_back(h);
  }
  std::sort(out.begin(), out.end(),
            [](const Hypothesis& a, const Hypothesis& b) { return a.score > b.score; });
  return out;
}

float wordConfidence(const std::vector<std::vector<float>>& log_probs,
                     const EnglishTokenizer& /*tok*/) {
  if (log_probs.empty()) return 0.0f;
  double acc = 0;
  for (auto& lp : log_probs) {
    float m = *std::max_element(lp.begin(), lp.end());
    acc += std::exp(m);
  }
  return static_cast<float>(acc / log_probs.size());
}

}  // namespace spoke
