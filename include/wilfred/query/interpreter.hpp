#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/query/classify.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct InterpretedQuery {
  QueryClass classification;
  std::vector<SearchResult> results;
};

class QueryInterpreter {
public:
  QueryInterpreter(IndexEngine& index, SearchEngine& search);

  InterpretedQuery interpret(const std::string& query, const Config& cfg,
                             HistoryStore* history);

  ProviderRegistry& providers() { return providers_; }

private:
  IndexEngine& index_;
  SearchEngine& search_;
  ProviderRegistry providers_;
};

bool execute_result(const SearchResult& r, const Config& cfg);
bool result_is_launchable(const SearchResult& r);

}  // namespace wilfred
