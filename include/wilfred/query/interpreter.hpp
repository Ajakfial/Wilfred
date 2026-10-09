#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/query/classify.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/snippets.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct InterpretedQuery {
  QueryClass classification;
  std::vector<SearchResult> results;
};

class QueryInterpreter {
public:
  QueryInterpreter(IndexEngine& index, SearchEngine& search, SnippetStore* snippets = nullptr,
                   PluginHost* plugins = nullptr);

  InterpretedQuery interpret(const std::string& query, const Config& cfg,
                             HistoryStore* history);
  // Fast phase of interpret(): the full pipeline minus the search-provider
  // fan-out (subprocesses / network). Used by the overlay so index hits
  // render instantly while providers resolve in the background; the
  // provider hits are merged later with merge_provider_results().
  // `effective` is the alias-expanded text providers run against;
  // `providers_apply` is false when the fast path returned early (math,
  // URL, commands, exact minis, ...) and providers must not run at all.
  struct FastResult {
    InterpretedQuery iq;
    std::string effective;
    bool providers_apply{false};
  };
  FastResult interpret_fast(const std::string& query, const Config& cfg,
                            HistoryStore* history);
  // Runs the registered providers for an already-interpreted query.
  std::vector<SearchResult> query_providers(const std::string& query, const Config& cfg,
                                            std::size_t limit);

  ProviderRegistry& providers() { return providers_; }

private:
  // Shared body for interpret() (full pipeline) and interpret_fast()
  // (everything except the provider fan-out). When out-params are given,
  // reports the alias-expanded provider text and whether execution reached
  // the provider stage (early returns leave providers_apply false).
  InterpretedQuery interpret_impl(const std::string& query, const Config& cfg,
                                  HistoryStore* history, bool include_providers,
                                  std::string* effective_out, bool* providers_apply_out);

  IndexEngine& index_;
  SearchEngine& search_;
  ProviderRegistry providers_;
  SnippetStore* snippets_{nullptr};
  PluginHost* plugins_{nullptr};
};

bool execute_result(const SearchResult& r, const Config& cfg, const std::string& action_id = {});
bool result_is_launchable(const SearchResult& r);

// Merges provider hits into a fast-phase result list: `os` hits whose path
// is already listed are dropped (Wilfred's own index ranks first) and
// repeats collapse. Shared by interpret() and the async overlay path so
// both rank identically.
void merge_provider_results(std::vector<SearchResult>& base, std::vector<SearchResult> extra);

}  // namespace wilfred
