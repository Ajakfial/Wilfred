#pragma once
#include <string>
#include <vector>

namespace spoke {

/// Bare-bones English tokenizer, from scratch (no SentencePiece).
/// Alphabet: 0=<blank>, 1-26=a-z, 27=' ' (space), 28='\'' (apostrophe).
/// Lowercases, strips everything else. Fully invertible on its alphabet.
class EnglishTokenizer {
 public:
  static constexpr int kBlank = 0;
  static constexpr int kVocabSize = 29;

  int vocabSize() const { return kVocabSize; }

  std::string normalize(const std::string& text) const;
  std::vector<int> encode(const std::string& text) const;
  std::string decode(const std::vector<int>& ids) const;

  char idToChar(int id) const;
  int charToId(char c) const;
  bool isValidId(int id) const { return id >= 0 && id < kVocabSize; }
};

}  // namespace spoke
