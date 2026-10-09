#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <vector>

namespace wilfred {

// Federated search against the official OS index so Wilfred cooperates with
// the platform instead of competing with it:
//
//   Windows -> Windows Search (native SystemIndex COM, Everything CLI,
//             PowerShell ADODB fallback)
//   macOS   -> Spotlight (mdfind)
//   Linux   -> Tracker (GNOME) / Baloo (KDE), locate/plocate fallback
//   BSD     -> locate
//   Android/iOS -> none (sandboxed, no subprocesses)
//
// The provider only fills gaps: Wilfred's own index ranks first and
// interpreter-level dedup drops OS hits whose path is already listed.

bool os_search_available();
std::string os_search_default_backend();
std::string os_search_backend_label(const std::string& backend);
std::vector<std::string> os_search_backends();
// Canonicalize user input ("mdfind" -> "spotlight", "es" -> "everything", ...).
std::string os_search_normalize_backend(const std::string& backend);
// Pure gate: long enough, no control chars, not oversized.
bool os_search_should_query(const std::string& text);
// Explicit `os <query>` escape hatch (case-insensitive "os " prefix).
bool os_search_strip_prefix(const std::string& text, std::string& remainder);
// Pure argv builder for one backend (empty = unsupported here).
// windows_search uses powershell -EncodedCommand (base64, no shell quoting).
std::vector<std::string> os_search_argv(const std::string& backend,
                                        const std::string& query, int limit);
// Pure output parsers (one path per hit, order preserved).
std::vector<std::string> os_search_parse_output(const std::string& backend,
                                                const std::string& output,
                                                std::size_t max_results);
// Pure Windows Search SQL builder (TOP n over SystemIndex).
std::string os_search_windows_sql(const std::string& query, int top);
#ifdef _WIN32
// Native in-process SystemIndex query (ISearchQueryHelper + ADO). True when
// the index answered (hits may be empty); false when unavailable so the
// caller can fall back to Everything CLI / PowerShell.
bool os_windows_search_com(const std::string& query, int limit, std::vector<std::string>& out);
#endif
std::vector<SearchResult> os_search_results_from_paths(
    const std::vector<std::string>& paths, const std::string& backend,
    std::size_t limit);

class OsSearchProvider : public SearchProvider {
 public:
  std::string id() const override { return "os"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

}  // namespace wilfred
