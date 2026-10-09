#pragma once

// Internal UI helpers shared by platform overlays.
#include "wilfred/search/engine.hpp"

#include <functional>
#include <string>
#include <vector>

namespace wilfred {

// Unified overlay response shared by all native hosts (Windows WebView2,
// macOS WKWebView, Linux WebKitGTK/layer-shell/X11). The web UI (ui/overlay)
// consumes the same JSON shape on every platform.
struct OverlayResponse {
  std::vector<SearchResult> results;
  // "Did you mean" rewrite for typo safety. Empty = no confident correction.
  std::string correction;
  // Inline ghost completion extending the raw query. Empty = none.
  std::string ghost;
  // Top-N autocomplete candidates for the dropdown. May be empty.
  std::vector<std::string> candidates;
  // Echo of the query that produced this response (for stale-race guard).
  std::string query;
};

using OverlaySubmit = std::function<void(const SearchResult&, const std::string& action_id)>;
using OverlayQuery = std::function<OverlayResponse(const std::string&)>;

void overlay_bind(OverlayQuery q, OverlaySubmit s);
void overlay_set_quit(std::function<void()> fn);
void overlay_pump();

// Late provider results for the async overlay path: replaces the displayed
// result list for resp.query without disturbing anything else. Each backend
// marshals to its own UI thread; backends without a visible overlay may
// leave it unimplemented. Thread-safe: callable from any thread.
void overlay_push_results(const OverlayResponse& resp);

}  // namespace wilfred
