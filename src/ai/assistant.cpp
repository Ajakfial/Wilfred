#include "wilfred/ai/assistant.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/json.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/search/engine.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

namespace wilfred {
namespace {

std::string lower_trim(const std::string& s) {
  auto t = normalize_query(s);
  return to_lower_utf8(t);
}

std::string https_post(const std::string& url, const std::string& body,
                       const std::vector<std::string>& headers, int timeout_ms,
                       int& status_out) {
  status_out = 0;
#ifdef _WIN32
  std::string rest = url;
  bool https = false;
  if (rest.rfind("https://", 0) == 0) {
    https = true;
    rest = rest.substr(8);
  } else if (rest.rfind("http://", 0) == 0) {
    rest = rest.substr(7);
  } else {
    return {};
  }
  auto slash = rest.find('/');
  std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
  std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
  auto colon = authority.find(':');
  std::wstring host =
      utf8_to_wide(colon == std::string::npos ? authority : authority.substr(0, colon));
  INTERNET_PORT port = https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT;
  if (colon != std::string::npos) {
    try {
      port = static_cast<INTERNET_PORT>(std::stoi(authority.substr(colon + 1)));
    } catch (...) {
    }
  }
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return {};
  WinHttpSetTimeouts(ses, timeout_ms / 2, timeout_ms / 2, timeout_ms, timeout_ms);
  HINTERNET con = WinHttpConnect(ses, host.c_str(), port, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return {};
  }
  auto wpath = utf8_to_wide(path);
  DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(con, L"POST", wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  std::string out;
  if (req) {
    std::wstring whdr;
    for (auto& h : headers) whdr += utf8_to_wide(h) + L"\r\n";
    if (WinHttpSendRequest(req, whdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : whdr.c_str(),
                           whdr.empty() ? 0 : (DWORD)-1, (LPVOID)body.data(),
                           (DWORD)body.size(), (DWORD)body.size(), 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
      DWORD code = 0, len = sizeof(code);
      if (WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &code, &len,
                              WINHTTP_NO_HEADER_INDEX))
        status_out = (int)code;
      DWORD avail = 0;
      while (WinHttpQueryDataAvailable(req, &avail) && avail) {
        std::string chunk(avail, '\0');
        DWORD got = 0;
        WinHttpReadData(req, chunk.data(), avail, &got);
        out.append(chunk.data(), got);
        if (out.size() > 256 * 1024) break;
      }
    }
    WinHttpCloseHandle(req);
  }
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return out;
#else
  // POSIX: curl handles TLS, redirects, and timeouts.
  std::string hdr;
  for (auto& h : headers) {
    std::string e;
    for (char c : h) {
      if (c == '\'')
        e += "'\\''";
      else
        e.push_back(c);
    }
    hdr += " -H '" + e + "'";
  }
  std::string esc_body;
  for (char c : body) {
    if (c == '\'')
      esc_body += "'\\''";
    else
      esc_body.push_back(c);
  }
  int secs = timeout_ms / 1000;
  if (secs < 3) secs = 3;
  if (secs > 120) secs = 120;
  std::string cmd = "curl -sS --max-time " + std::to_string(secs) + hdr +
                    " -d '" + esc_body + "' '" + url + "' 2>/dev/null";
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) return {};
  std::string resp;
  char buf[4096];
  while (fgets(buf, sizeof(buf), f)) {
    resp += buf;
    if (resp.size() > 256 * 1024) break;
  }
  int rc = pclose(f);
  status_out = (rc == 0 && !resp.empty()) ? 200 : 0;
  return resp;
#endif
}

// Extract assistant text from the major response shapes.
std::string extract_answer_text(const std::string& provider, const std::string& body) {
  // OpenAI / Groq (chat completions): choices[0].message.content
  // Anthropic: content[0].text ; Gemini: candidates[0].content.parts[0].text
  // We do tolerant substring extraction instead of full JSON parsing.
  auto grab = [&](const char* key) -> std::string {
    std::string k = std::string("\"") + key + "\"";
    auto pos = body.find(k);
    if (pos == std::string::npos) return {};
    pos = body.find(':', pos + k.size());
    if (pos == std::string::npos) return {};
    ++pos;
    while (pos < body.size() && (body[pos] == ' ' || body[pos] == '\t')) ++pos;
    if (pos >= body.size() || body[pos] != '"') return {};
    ++pos;
    std::string v;
    while (pos < body.size() && body[pos] != '"') {
      if (body[pos] == '\\' && pos + 1 < body.size()) {
        char n = body[pos + 1];
        if (n == 'n') v.push_back('\n');
        else if (n == 't') v.push_back('\t');
        else if (n == 'r') v.push_back('\r');
        else v.push_back(n);
        pos += 2;
      } else {
        v.push_back(body[pos++]);
      }
      if (v.size() > 8192) break;
    }
    return v;
  };
  (void)provider;
  // Prefer the most specific keys first.
  std::string t = grab("text");
  if (!t.empty()) return t;
  std::string c = grab("content");
  if (!c.empty() && c.find("{") == std::string::npos) return c;
  return {};
}

}  // namespace

bool ai_query_is_request(const std::string& query, std::string& prompt_out) {
  auto l = lower_trim(query);
  const char* prefixes[] = {"ai ", "ask ", "gpt ", "assistant ", "ai:", "ask:"};
  for (auto* p : prefixes) {
    std::size_t n = std::strlen(p);
    if (l.rfind(p, 0) == 0) {
      std::string raw = normalize_query(query);
      prompt_out = raw.size() > n ? raw.substr(n) : std::string();
      while (!prompt_out.empty() && prompt_out.front() == ' ') prompt_out.erase(prompt_out.begin());
      while (!prompt_out.empty() && prompt_out.front() == ':') prompt_out.erase(prompt_out.begin());
      while (!prompt_out.empty() && prompt_out.front() == ' ') prompt_out.erase(prompt_out.begin());
      return !prompt_out.empty();
    }
  }
  if (l == "ai" || l == "ask") {
    prompt_out.clear();
    return true;  // bare prefix => hint card
  }
  return false;
}

std::string ai_provider_default_model(const std::string& provider) {
  auto p = to_lower_utf8(provider);
  if (p == "anthropic") return "claude-3-5-sonnet-latest";
  if (p == "gemini" || p == "google") return "gemini-1.5-flash";
  if (p == "groq") return "llama-3.1-8b-instant";
  return "gpt-4o-mini";
}

std::string ai_provider_default_endpoint(const std::string& provider) {
  auto p = to_lower_utf8(provider);
  if (p == "anthropic") return "https://api.anthropic.com/v1/messages";
  if (p == "gemini" || p == "google") return "";  // model-specific, built at call time
  if (p == "groq") return "https://api.groq.com/openai/v1/chat/completions";
  return "https://api.openai.com/v1/chat/completions";
}

bool AiAssistant::configured(const Config& cfg) const {
  if (!cfg.ai.enabled) return false;
  // Custom endpoint without a key is allowed (self-hosted proxy).
  if (!cfg.ai.endpoint.empty()) return true;
  return !cfg.ai.api_key.empty();
}

std::string AiAssistant::active_provider(const Config& cfg) const {
  auto p = to_lower_utf8(cfg.ai.provider);
  if (p.empty() || p == "auto") {
    // Guess from endpoint, default to OpenAI-compatible.
    auto e = to_lower_utf8(cfg.ai.endpoint);
    if (e.find("anthropic") != std::string::npos) return "anthropic";
    if (e.find("gemini") != std::string::npos || e.find("google") != std::string::npos)
      return "gemini";
    if (e.find("groq") != std::string::npos) return "groq";
    return "openai";
  }
  if (p == "google") return "gemini";
  return p;
}

std::string AiAssistant::active_model(const Config& cfg) const {
  if (!cfg.ai.model.empty()) return cfg.ai.model;
  return ai_provider_default_model(active_provider(cfg));
}

AiAnswer AiAssistant::ask(const std::string& prompt, const Config& cfg) const {
  AiAnswer ans;
  if (!configured(cfg)) {
    ans.error = "AI assistant is off (set ai.enabled + ai.api_key)";
    return ans;
  }
  if (prompt.empty()) {
    ans.error = "empty prompt";
    return ans;
  }
  std::string provider = active_provider(cfg);
  std::string model = active_model(cfg);
  ans.model = model;
  int timeout = cfg.ai.timeout_ms > 0 ? cfg.ai.timeout_ms : 30000;
  int max_tokens = cfg.ai.max_tokens > 0 ? cfg.ai.max_tokens : 1024;

  std::string url;
  std::string body;
  std::vector<std::string> headers;
  headers.push_back("Content-Type: application/json");

  if (provider == "anthropic") {
    url = cfg.ai.endpoint.empty() ? ai_provider_default_endpoint("anthropic") : cfg.ai.endpoint;
    headers.push_back("x-api-key: " + cfg.ai.api_key);
    headers.push_back("anthropic-version: 2023-06-01");
    body = "{\"model\":\"" + json_escape(model) + "\",\"max_tokens\":" +
           std::to_string(max_tokens) + ",\"messages\":[{\"role\":\"user\",\"content\":\"" +
           json_escape(prompt) + "\"}]}";
  } else if (provider == "gemini") {
    std::string base = cfg.ai.endpoint;
    if (base.empty()) {
      base = "https://generativelanguage.googleapis.com/v1beta/models/" + model +
             ":generateContent?key=" + cfg.ai.api_key;
    } else if (base.find("key=") == std::string::npos && !cfg.ai.api_key.empty()) {
      base += (base.find('?') == std::string::npos ? "?" : "&") + std::string("key=") +
              cfg.ai.api_key;
    }
    url = base;
    body = "{\"contents\":[{\"parts\":[{\"text\":\"" + json_escape(prompt) + "\"}]}]}";
  } else {
    // OpenAI-compatible (OpenAI + Groq + self-hosted).
    if (!cfg.ai.endpoint.empty())
      url = cfg.ai.endpoint;
    else if (provider == "groq")
      url = ai_provider_default_endpoint("groq");
    else
      url = ai_provider_default_endpoint("openai");
    if (!cfg.ai.api_key.empty()) headers.push_back("Authorization: Bearer " + cfg.ai.api_key);
    char tmp[64];
    std::snprintf(tmp, sizeof(tmp), "%.2f", cfg.ai.temperature);
    body = "{\"model\":\"" + json_escape(model) + "\",\"max_tokens\":" +
           std::to_string(max_tokens) + ",\"temperature\":" + tmp +
           ",\"messages\":[{\"role\":\"user\",\"content\":\"" + json_escape(prompt) + "\"}]}";
  }

  int status = 0;
  std::string resp = https_post(url, body, headers, timeout, status);
  if (resp.empty()) {
    ans.error = "request failed (offline or bad endpoint)";
    return ans;
  }
  std::string text = extract_answer_text(provider, resp);
  if (text.empty()) {
    // Surface a short server error instead of raw JSON.
    std::string em = json_get_string(resp, "error");
    if (em.empty()) em = json_get_string(resp, "message");
    ans.error = em.empty() ? "empty response" : em.substr(0, 240);
    return ans;
  }
  ans.ok = true;
  ans.text = text;
  return ans;
}

std::vector<SearchResult> AiAssistant::results_for(const std::string& prompt,
                                                  const Config& cfg) const {
  std::vector<SearchResult> out;
  if (prompt.empty()) {
    SearchResult hint;
    hint.title = "Ask the AI assistant";
    hint.subtitle = "Type `ai your question` · needs ai.api_key";
    hint.payload = "";
    hint.path = "";
    hint.action = ResultAction::Copy;
    hint.score = 7000;
    hint.kind_label = "ai";
    hint.category = "ai";
    out.push_back(std::move(hint));
    return out;
  }
  if (!configured(cfg)) {
    SearchResult hint;
    hint.title = "AI assistant is not configured";
    hint.subtitle = "Set ai.enabled: true and ai.api_key in wilfred.yml";
    hint.payload = prompt;
    hint.path = "";
    hint.action = ResultAction::Copy;
    hint.score = 6000;
    hint.kind_label = "ai";
    hint.category = "ai";
    out.push_back(std::move(hint));
    return out;
  }
  AiAnswer ans = ask(prompt, cfg);
  SearchResult r;
  if (ans.ok) {
    r.title = ans.text.size() > 220 ? ans.text.substr(0, 220) + "…" : ans.text;
    r.subtitle = "AI · " + ans.model + " · enter copies";
    r.payload = ans.text;
    r.score = 9500;
  } else {
    r.title = "AI request failed";
    r.subtitle = ans.error + " · enter copies prompt";
    r.payload = prompt;
    r.score = 5000;
  }
  r.path = "";
  r.action = ResultAction::Copy;
  r.kind_label = "ai";
  r.category = "ai";
  out.push_back(std::move(r));
  return out;
}

}  // namespace wilfred
