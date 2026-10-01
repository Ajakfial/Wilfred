#pragma once

// Optional local AI assistant. Disabled unless the user provides an API key
// in `ai.api_key` (or `ai.endpoint` for a self-hosted OpenAI-compatible
// server). Supports OpenAI, Anthropic, Gemini, and Groq over HTTPS.
//
// Design notes:
//  - No API key is ever logged; keys live only in the user config file.
//  - HTTPS uses WinHTTP on Windows and the `curl` CLI on macOS/Linux so the
//    binary stays dependency-free and cross-platform.
//  - Queries are prefixed (`ai ...`, `ask ...`) and return a single copyable
//    card; failures produce a terse, non-blocking hint instead of an error.

#include <string>
#include <vector>

namespace wilfred {

struct Config;
struct SearchResult;

bool ai_query_is_request(const std::string& query, std::string& prompt_out);
std::string ai_provider_default_model(const std::string& provider);
std::string ai_provider_default_endpoint(const std::string& provider);

struct AiAnswer {
  bool ok{false};
  std::string text;
  std::string model;
  std::string error;
};

class AiAssistant {
 public:
  bool configured(const Config& cfg) const;
  std::string active_provider(const Config& cfg) const;
  std::string active_model(const Config& cfg) const;

  // Blocking network call (honors ai.timeout_ms). Never throws.
  AiAnswer ask(const std::string& prompt, const Config& cfg) const;

  // Build overlay results for an `ai ...` query (answer card + hint card).
  std::vector<SearchResult> results_for(const std::string& prompt, const Config& cfg) const;
};

}  // namespace wilfred
