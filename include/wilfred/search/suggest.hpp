#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace wilfred {

struct Config;
class HistoryStore;
class IndexEngine;

// Typo-tolerant helpers shared by fuzzy matching, command parsing,
// query interpretation and autocomplete.
//
// All functions are side-effect-free and cheap enough to call per keystroke
// on the overlay path.

// Full command vocabulary used for correction + autocomplete.
// Includes mini commands, system actions, macros, filters and prefixes.
std::vector<std::string> command_vocabulary(const Config& cfg);

// Closest vocabulary entry to key within typo distance, or empty.
std::string correct_command_key(std::string_view key, const Config& cfg);

// Correct the leading command token of a query ("weahter london" ->
// "weather london"). Returns empty when no confident correction exists.
std::string correct_query_command(std::string_view query, const Config& cfg);

// Best full-query correction considering history + vocabulary.
// Returns empty when the query looks fine or no confident fix exists.
std::string suggest_correction(const std::string& query, const Config& cfg, HistoryStore* history);

// Ghost completion: full text that extends query (query must be a prefix,
// case-insensitive, or a close typo-prefix). Returns empty when none.
std::string autocomplete_ghost(const std::string& query, const Config& cfg, HistoryStore* history);

// Top-N inline candidates for the autocomplete dropdown.
std::vector<std::string> autocomplete_candidates(const std::string& query, const Config& cfg,
                                                 HistoryStore* history, int n = 6);

// Combined assist payload sent to the overlay with every results message.
// Cross-platform: Windows (WebView2), macOS (WKWebView) and Linux
// (WebKitGTK/layer-shell/X11) all consume the same fields.
struct AssistResult {
  // Confident "did you mean" rewrite ("weahter" -> "weather"). Empty = none.
  std::string correction;
  // Inline ghost completion that extends the current query ("wea" -> "weather").
  // Empty = none. The UI renders ghost as dim suffix + Tab/Right to accept.
  std::string ghost;
  // Top-N dropdown candidates (history + vocabulary + index-aware, deduped).
  std::vector<std::string> candidates;
};

// Build the full assist payload for a raw overlay query. Never throws;
// returns empty fields when nothing confident exists.
AssistResult build_assist(const std::string& query, const Config& cfg, HistoryStore* history,
                          IndexEngine* index = nullptr);

// Index-aware filename/app completions merged into autocomplete.
// Returns up to n Display names (basename or app title) that extend query.
std::vector<std::string> index_autocomplete(const std::string& query, IndexEngine& index,
                                            int n = 4);

// Closest indexed filename within typo distance (for "did you mean" on files).
// Returns the display name or empty.
std::string correct_index_name(const std::string& query, IndexEngine& index);

}  // namespace wilfred
