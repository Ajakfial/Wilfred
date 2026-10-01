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
  std::string kind;    // text | clip
  std::string folder;  // optional group shown as "Folder / trigger"
};

class SnippetStore {
public:
  bool load(const std::string& path, const Config& cfg);
  bool save() const;
  void upsert(Snippet s);
  bool remove(const std::string& id);

  const std::vector<Snippet>& all() const { return items_; }
  std::vector<SearchResult> match(const std::string& query, const Config& cfg) const;
  const Snippet* find_trigger(const std::string& trigger) const;
  std::string path() const { return path_; }

private:
  std::string path_;
  std::vector<Snippet> items_;
};

std::string default_snippets_path();
bool query_is_snippet_save(const std::string& query, std::string& name_out);
bool snippet_query_forced(const std::string& query, const Config& cfg);

// Expand placeholders in a snippet body:
//   {date} {time} {datetime} {year} {month} {day} {clipboard} {query}
// Unknown placeholders are left untouched.
std::string expand_snippet_placeholders(const std::string& body, const std::string& query_text,
                                        const std::string& clipboard_text);

// Global abbreviation expansion: does `typed` end with a snippet trigger
// followed by a delimiter (space/tab/enter/punctuation)? If so returns the
// snippet and sets `trigger_len_out` to the trigger length.
const Snippet* snippet_global_match(const SnippetStore& store, const std::string& typed,
                                    std::size_t& trigger_len_out);

}  // namespace wilfred
