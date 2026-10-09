#pragma once

#include "wilfred/providers/provider.hpp"

#include <string>
#include <vector>

namespace wilfred {

struct LibraryItem {
  std::string title;
  std::string url;
  std::string source;  // "bookmark", "history", or "tab"
};

// Parse one Chromium Bookmarks JSON file, appending up to cap items.
// Exposed for testing; browser_library() discovers the profile files.
bool parse_chromium_bookmarks_file(const std::string& file, std::vector<LibraryItem>& out,
                                   std::size_t cap);

// All known browser bookmarks, recent history entries, and (where the OS
// allows) open tabs. Results are cached for 60s. Never throws.
std::vector<LibraryItem> browser_library(bool include_history, bool include_tabs);

// SearchProvider over browser_library(): substring match on title/URL.
class BrowserLibraryProvider : public SearchProvider {
public:
  std::string id() const override { return "browser-library"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

}  // namespace wilfred
