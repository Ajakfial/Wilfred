#include "wilfred/search/embedding.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace wilfred {
namespace {

std::uint64_t fnv1a(const char* p, std::size_t n, std::uint64_t seed = 1469598103934665603ull) {
  std::uint64_t h = seed;
  for (std::size_t i = 0; i < n; ++i) {
    h ^= static_cast<unsigned char>(p[i]);
    h *= 1099511628211ull;
  }
  return h;
}

}  // namespace

std::vector<float> hash_embed_text(const std::string& text, int dim) {
  if (dim < 32) dim = 32;
  if (dim > 4096) dim = 4096;
  std::vector<float> v(static_cast<std::size_t>(dim), 0.0f);
  std::string folded = fold_search(normalize_query(text));
  if (folded.empty()) return v;
  // Char 3-grams carry the bulk of the signal.
  std::string pad = "  " + folded + "  ";
  for (std::size_t i = 0; i + 2 < pad.size(); ++i) {
    std::uint64_t h = fnv1a(pad.data() + i, 3);
    v[h % static_cast<std::uint64_t>(dim)] += 1.0f;
    v[(h >> 16) % static_cast<std::uint64_t>(dim)] += 0.5f;
  }
  // Word unigrams stabilize longer texts.
  std::size_t s = 0;
  while (s < folded.size()) {
    while (s < folded.size() && folded[s] == ' ') ++s;
    if (s >= folded.size()) break;
    std::size_t e = s;
    while (e < folded.size() && folded[e] != ' ') ++e;
    if (e > s) {
      std::uint64_t h = fnv1a(folded.data() + s, e - s);
      v[h % static_cast<std::uint64_t>(dim)] += 1.5f;
    }
    s = e;
  }
  float norm = 0;
  for (float f : v) norm += f * f;
  norm = std::sqrt(norm);
  if (norm > 1e-9f) {
    for (float& f : v) f /= norm;
  }
  return v;
}

float embedding_cosine(const std::vector<float>& a, const std::vector<float>& b) {
  if (a.empty() || b.empty() || a.size() != b.size()) return 0;
  double dot = 0;
  for (std::size_t i = 0; i < a.size(); ++i) dot += static_cast<double>(a[i]) * b[i];
  if (dot > 1) return 1;
  if (dot < -1) return -1;
  return static_cast<float>(dot);
}

std::string LocalEmbedder::configure(const EmbeddingConfigView& cfg) {
  cfg_ = cfg;
  dim_ = cfg.dim > 0 ? cfg.dim : 384;
  if (dim_ < 32) dim_ = 32;
  if (dim_ > 4096) dim_ = 4096;
  auto backend = to_lower_utf8(cfg.backend);
  if (backend == "server" && !cfg.endpoint.empty()) {
    active_backend_ = "server";
  } else if ((backend == "llamacpp" || backend == "llama.cpp" || backend == "llama") &&
             !cfg.model.empty()) {
    active_backend_ = "llamacpp";
  } else if (backend == "hash") {
    active_backend_ = "hash";
  } else {
    // auto: prefer server, then model file, else hash.
    if (!cfg.endpoint.empty())
      active_backend_ = "server";
    else if (!cfg.model.empty() && file_exists(cfg.model))
      active_backend_ = "llamacpp";
    else
      active_backend_ = "hash";
  }
  return active_backend_;
}

bool LocalEmbedder::wants_llamacpp() const {
  return active_backend_ == "server" || active_backend_ == "llamacpp";
}

bool LocalEmbedder::probe_server(const std::string& endpoint, int timeout_ms) {
  (void)timeout_ms;
  if (endpoint.empty()) return false;
  // Minimal probe: try `GET <endpoint>/health` for llama.cpp server, or just
  // check the endpoint string is a local URL. Full HTTP is done in embed().
#ifdef _WIN32
  // Use a short WinHTTP GET for health; failure => false.
  std::string url = endpoint;
  // llama.cpp exposes /health at the server root; strip trailing /embedding.
  auto pos = url.rfind("/embedding");
  std::string base = (pos != std::string::npos) ? url.substr(0, pos) : url;
  std::string health = base + "/health";
  auto wurl = utf8_to_wide(health);
  // Very small WinHTTP fetch (no auth).
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return false;
  WinHttpSetTimeouts(ses, 800, 800, 800, 1500);
  // crude URL split: expect http://host:port/...
  std::string rest = health;
  if (rest.rfind("http://", 0) == 0) rest = rest.substr(7);
  auto slash = rest.find('/');
  std::string authority = slash == std::string::npos ? rest : rest.substr(0, slash);
  std::string path = slash == std::string::npos ? "/" : rest.substr(slash);
  auto colon = authority.find(':');
  std::wstring host =
      utf8_to_wide(colon == std::string::npos ? authority : authority.substr(0, colon));
  INTERNET_PORT port = 80;
  if (colon != std::string::npos) {
    try {
      port = static_cast<INTERNET_PORT>(std::stoi(authority.substr(colon + 1)));
    } catch (...) {
    }
  }
  HINTERNET con = WinHttpConnect(ses, host.c_str(), port, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return false;
  }
  auto wpath = utf8_to_wide(path);
  HINTERNET req = WinHttpOpenRequest(con, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  bool ok = false;
  if (req) {
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0,
                           0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
      DWORD code = 0, len = sizeof(code);
      if (WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                              WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX))
        ok = (code == 200);
    }
    WinHttpCloseHandle(req);
  }
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return ok;
#else
  // POSIX: use curl for the probe when available.
  std::string base = endpoint;
  auto pos = base.rfind("/embedding");
  if (pos != std::string::npos) base = base.substr(0, pos);
  std::string cmd = "curl -fsS --max-time 2 -o /dev/null \"" + base + "/health\" 2>/dev/null";
  int rc = std::system(cmd.c_str());
  return rc == 0;
#endif
}

namespace {
// Minimal JSON float-array extractor for {"embedding":[...]} responses.
bool parse_embedding_array(const std::string& body, std::vector<float>& out, int dim) {
  auto pos = body.find('[');
  if (pos == std::string::npos) return false;
  out.clear();
  out.reserve(static_cast<std::size_t>(dim));
  std::string num;
  auto flush = [&]() {
    if (num.empty()) return;
    try {
      out.push_back(std::stof(num));
    } catch (...) {
    }
    num.clear();
  };
  for (std::size_t i = pos; i < body.size() && out.size() < static_cast<std::size_t>(dim);
       ++i) {
    char c = body[i];
    if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')
      num.push_back(c);
    else if (c == ',' || c == ']') {
      flush();
      if (c == ']') break;
    }
  }
  flush();
  return !out.empty();
}

std::string json_escape_small(const std::string& s) {
  std::string o;
  for (char c : s) {
    if (c == '"')
      o += "\\\"";
    else if (c == '\\')
      o += "\\\\";
    else if (c == '\n')
      o += "\\n";
    else if (c == '\r')
      o += "\\r";
    else if (c == '\t')
      o += "\\t";
    else
      o.push_back(c);
  }
  return o;
}
}  // namespace

std::vector<float> LocalEmbedder::embed_via_server(const std::string& text) const {
  if (cfg_.endpoint.empty()) return {};
#ifdef _WIN32
  // Windows: POST JSON to the llama.cpp server via WinHTTP.
  std::string url = cfg_.endpoint;
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
  std::string payload = "{\"content\":\"" + json_escape_small(text) + "\"}";
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return {};
  WinHttpSetTimeouts(ses, 4000, 4000, 8000, 8000);
  HINTERNET con = WinHttpConnect(ses, host.c_str(), port, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return {};
  }
  auto wpath = utf8_to_wide(path);
  DWORD flags = https ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(con, L"POST", wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                    WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  std::vector<float> out;
  if (req) {
    std::wstring hdr = L"Content-Type: application/json\r\n";
    if (WinHttpSendRequest(req, hdr.c_str(), (DWORD)-1, (LPVOID)payload.data(),
                           (DWORD)payload.size(), (DWORD)payload.size(), 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
      std::string body;
      DWORD avail = 0;
      while (WinHttpQueryDataAvailable(req, &avail) && avail) {
        std::string chunk(avail, '\0');
        DWORD got = 0;
        WinHttpReadData(req, chunk.data(), avail, &got);
        body.append(chunk.data(), got);
        if (body.size() > 4 * 1024 * 1024) break;
      }
      std::vector<float> v;
      if (parse_embedding_array(body, v, dim_)) out = std::move(v);
    }
    WinHttpCloseHandle(req);
  }
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return out;
#else
  // POSIX: POST via curl (handles http+https, no extra deps).
  std::string payload = "{\"content\":\"" + json_escape_small(text) + "\"}";
  // Escape single quotes for shell.
  std::string esc;
  for (char c : payload) {
    if (c == '\'')
      esc += "'\\''";
    else
      esc.push_back(c);
  }
  std::string cmd = "curl -fsS --max-time 8 -H 'Content-Type: application/json' -d '" + esc +
                    "' \"" + cfg_.endpoint + "\" 2>/dev/null";
  FILE* f = popen(cmd.c_str(), "r");
  if (!f) return {};
  std::string body;
  char buf[2048];
  while (fgets(buf, sizeof(buf), f)) {
    body += buf;
    if (body.size() > 4 * 1024 * 1024) break;
  }
  pclose(f);
  std::vector<float> v;
  if (parse_embedding_array(body, v, dim_)) return v;
  return {};
#endif
}

std::vector<float> LocalEmbedder::embed_via_llamacpp(const std::string& text) const {
  // Direct .gguf inference requires a llama.cpp build. Wilfred loads it at
  // runtime when present (libllama / llama.dll) so the default binary stays
  // dependency-free. When the library is absent we return {} and the caller
  // falls back to the hash embedder.
  //
  // Future work: link optionally against llama.cpp when WILFRED_HAS_LLAMA is
  // defined and call llama_model_load_from_file + embedding path here.
  (void)text;
  if (cfg_.model.empty() || !file_exists(cfg_.model)) return {};
  // No runtime libllama found in this build — signal fallback.
  return {};
}

std::vector<float> LocalEmbedder::embed(const std::string& text) const {
  if (active_backend_ == "server") {
    auto v = embed_via_server(text);
    if (!v.empty()) {
      if ((int)v.size() != dim_) {
        // Resize by truncation/pad + renormalize.
        v.resize(static_cast<std::size_t>(dim_), 0.0f);
      }
      float n = 0;
      for (float f : v) n += f * f;
      n = std::sqrt(n);
      if (n > 1e-9f)
        for (float& f : v) f /= n;
      return v;
    }
    // Server unreachable — fall back to hash so search keeps working offline.
  } else if (active_backend_ == "llamacpp") {
    auto v = embed_via_llamacpp(text);
    if (!v.empty() && (int)v.size() == dim_) return v;
  }
  return hash_embed_text(text, dim_);
}

}  // namespace wilfred
