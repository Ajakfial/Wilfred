#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/search/engine.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {

// A user-configured remote search backend (opt-in only).
// url is a GET template with {query} / {query_enc} placeholders, e.g.
//   https://wiki.example.com/api/search?q={query_enc}
// The endpoint must return JSON: either
//   {"results":[{"title":"..","subtitle":"..","url":"..","score":500}, ...]}
// or a bare array [...] with the same objects. Unknown fields are ignored.
struct RemoteSource {
  std::string name;
  std::string url;
};

// Pure helpers (unit-tested, no network).
std::string remote_url_for(const std::string& tmpl, const std::string& query);
std::vector<SearchResult> remote_parse_response(const std::string& body,
                                                const std::string& source_name,
                                                const std::string& query);
// Fetch one URL with a bounded timeout. Empty on failure. Uses WinHTTP on
// Windows, `curl --max-time` elsewhere. Headers are sent as-is (caller must
// have validated them); they are never logged.
std::string remote_fetch(const std::string& url, int timeout_ms,
                         const std::unordered_map<std::string, std::string>& headers = {});

class RemoteProvider : public SearchProvider {
public:
  std::string id() const override { return "remote"; }
  std::vector<SearchResult> query(const std::string& text, const Config& cfg,
                                  std::size_t limit) override;
};

}  // namespace wilfred
