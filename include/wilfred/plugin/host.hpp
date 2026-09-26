#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <memory>
#include <string>
#include <vector>

namespace wilfred {

struct PluginManifest {
  std::string id;
  std::string kind;  // native | stdio
  std::string path;
  std::string command;
  std::vector<std::string> args;
  std::string directory;
  int timeout_ms{400};
  bool enabled{true};
};

std::vector<SearchResult> parse_plugin_results_json(const std::string& json,
                                                    const std::string& plugin_id);
std::string plugin_query_json(const std::string& q, std::size_t limit);

class PluginHost {
public:
  PluginHost();
  ~PluginHost();

  void load(const Config& cfg);
  void unload();
  std::vector<SearchResult> query(const std::string& text, const Config& cfg, std::size_t limit);
  bool execute(const SearchResult& r, const std::string& action_id);
  const std::vector<PluginManifest>& manifests() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

std::vector<std::string> default_plugin_directories();

}  // namespace wilfred
