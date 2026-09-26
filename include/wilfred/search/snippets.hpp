#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct Snippet {
  std::string id;
  std::string trigger;
  std::string title;
  std::string body;
  std::string kind;  // text | clip
};

class SnippetStore {
public:
  bool load(const std::string& path, const Config& cfg);
  bool save() const;
  void upsert(Snippet s);
  bool remove(const std::string& id);

  const std::vector<Snippet>& all() const { return items_; }
  std::vector<SearchResult> match(const std::string& query, const Config& cfg) const;
  std::string path() const { return path_; }

private:
  std::string path_;
  std::vector<Snippet> items_;
};

std::string default_snippets_path();
bool query_is_snippet_save(const std::string& query, std::string& name_out);
bool snippet_query_forced(const std::string& query, const Config& cfg);

}  // namespace wilfred
