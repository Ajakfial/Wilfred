#pragma once

#include "wilfred/index/engine.hpp"
#include "wilfred/providers/provider.hpp"

#include <string>
#include <vector>

namespace wilfred {

// Character-trigram cosine similarity in [0,1] over folded text.
// Dependency-free soft-match signal behind the optional semantic provider.
double trigram_cosine(const std::string& a, const std::string& b);

// Optional name/path soft-match backend. Scores every live index record by
// trigram similarity to the query and returns those above
// cfg.providers.semantic_min_score. Off unless enabled in config.
class SemanticProvider : public SearchProvider {
 public:
  explicit SemanticProvider(IndexEngine& index) : index_(index) {}
  std::string id() const override { return "semantic"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;

 private:
  IndexEngine& index_;
};

}  // namespace wilfred
