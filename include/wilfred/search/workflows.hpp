#pragma once

#include "wilfred/config/config.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct SearchResult;

// Quicklinks: parameterized URL/path/command templates from config.quicklinks
// merged with macros. Placeholders: {query} {query_enc} {clipboard}
// {clipboard_enc} plus positional {1} {2} ... and {*} (all args).
struct QuicklinkMatch {
  std::string name;
  std::string tmpl;
  std::string args;
  bool matched{false};
};

QuicklinkMatch match_quicklink(const std::string& query, const Config& cfg);
std::string expand_quicklink(const std::string& tmpl, const std::string& args,
                             const std::string& clipboard);
std::vector<SearchResult> quicklink_results(const QuicklinkMatch& m, const std::string& clipboard);

// Workflows: named multi-step action chains from config.workflows.
// `workflow <name>`, `flow <name>`, `run <name>`, `workflows` (list).
bool is_workflow_query(const std::string& query, const Config& cfg, std::string& out_name);
std::vector<SearchResult> workflow_results(const std::string& name, const Config& cfg);
std::string workflow_chain(const std::string& name, const Config& cfg);

}  // namespace wilfred
