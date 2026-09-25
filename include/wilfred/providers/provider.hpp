#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <memory>
#include <string>
#include <vector>

namespace wilfred {

// Extension point for additional search backends (cloud storage, IDEs, plugins).
class SearchProvider {
public:
  virtual ~SearchProvider() = default;
  virtual std::string id() const = 0;
  virtual std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                          std::size_t limit) = 0;
};

class ProviderRegistry {
public:
  void add(std::unique_ptr<SearchProvider> p) { providers_.push_back(std::move(p)); }
  const std::vector<std::unique_ptr<SearchProvider>>& all() const { return providers_; }

  std::vector<SearchResult> query_all(const std::string& text, const Config& cfg,
                                      std::size_t limit) const {
    std::vector<SearchResult> out;
    for (auto& p : providers_) {
      try {
        auto part = p->query(text, cfg, limit);
        out.insert(out.end(), part.begin(), part.end());
      } catch (...) {
      }
    }
    return out;
  }

private:
  std::vector<std::unique_ptr<SearchProvider>> providers_;
};

}  // namespace wilfred
