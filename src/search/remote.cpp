#include "wilfred/search/remote.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cstdio>

#ifdef _WIN32
// Order matters: winsock2/windows before winhttp.
#include <windows.h>
#include <winhttp.h>
#endif

namespace wilfred {

std::string remote_url_for(const std::string& tmpl, const std::string& query) {
  std::string out = tmpl;
  auto enc = percent_encode(query);
  auto subst_all = [&](const std::string& from, const std::string& to) {
    std::size_t pos = 0;
    while ((pos = out.find(from, pos)) != std::string::npos) {
      out.replace(pos, from.size(), to);
      pos += to.size();
    }
  };
  subst_all("{query_enc}", enc);
  subst_all("{q_enc}", enc);
  subst_all("{query}", query);
  subst_all("{q}", query);
  // No placeholder: append as ?q= for convenience.
  if (tmpl.find("{query") == std::string::npos && tmpl.find("{q") == std::string::npos) {
    out += (out.find('?') == std::string::npos ? "?q=" : "&q=") + enc;
  }
  return out;
}

std::vector<SearchResult> remote_parse_response(const std::string& body,
                                                const std::string& source_name,
                                                const std::string& query) {
  std::vector<SearchResult> out;
  if (body.empty() || body.size() > 512 * 1024) return out;
  std::string arr = json_extract_array(body, "results");
  std::string items_src = arr.empty() ? body : arr;
  // Bare object with results?
  if (items_src.empty()) return out;
  auto objs = json_object_array(items_src);
  // Also accept a single object.
  if (objs.empty() && !body.empty() && body.find('{') != std::string::npos &&
      json_get_string(body, "title").size() > 0) {
    objs.push_back(body);
  }
  for (auto& o : objs) {
    if (out.size() >= 20) break;
    std::string title = json_get_string(o, "title");
    if (title.empty()) title = json_get_string(o, "name");
    std::string url = json_get_string(o, "url");
    if (url.empty()) url = json_get_string(o, "link");
    if (url.empty()) url = json_get_string(o, "path");
    std::string sub = json_get_string(o, "subtitle");
    if (sub.empty()) sub = json_get_string(o, "snippet");
    if (sub.empty()) sub = json_get_string(o, "description");
    if (title.empty()) {
      if (!url.empty())
        title = url;
      else
        continue;
    }
    SearchResult r;
    r.title = title;
    r.subtitle = (sub.empty() ? source_name : sub + " · " + source_name);
    if (r.subtitle.empty()) r.subtitle = source_name.empty() ? query : source_name;
    r.path = url.empty() ? title : url;
    r.payload = r.path;
    r.action = url.empty() ? ResultAction::Copy : ResultAction::WebSearch;
    if (!url.empty() && url[0] == '/' ) r.action = ResultAction::Open;
    r.score = 400 + json_get_int(o, "score", 100);
    if (r.score < 1) r.score = 400;
    if (r.score > 5000) r.score = 5000;
    r.kind_label = "remote";
    r.category = "remote";
    r.plugin_id = "";
    out.push_back(std::move(r));
  }
  return out;
}

std::string remote_fetch(const std::string& url, int timeout_ms) {
  if (url.empty() || url.size() > 2048) return {};
  if (url.rfind("http://", 0) != 0 && url.rfind("https://", 0) != 0) return {};
  if (timeout_ms < 1000) timeout_ms = 1000;
  if (timeout_ms > 30000) timeout_ms = 30000;
#ifdef _WIN32
  // Minimal WinHTTP GET. Only https/http, no auth.
  std::string host, path;
  bool secure = url.rfind("https://", 0) == 0;
  std::string rest = url.substr(secure ? 8 : 7);
  auto slash = rest.find('/');
  if (slash == std::string::npos) {
    host = rest;
    path = "/";
  } else {
    host = rest.substr(0, slash);
    path = rest.substr(slash);
  }
  auto qm = path.find('?');
  // Strip fragment.
  auto hash = path.find('#');
  if (hash != std::string::npos) path.resize(hash);
  if (path.empty()) path = "/";
  auto whost = utf8_to_wide(host);
  auto wpath = utf8_to_wide(path);
  (void)qm;
  std::string body;
  HINTERNET ses = WinHttpOpen(L"Wilfred/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                              WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!ses) return {};
  int secs = timeout_ms / 1000;
  WinHttpSetTimeouts(ses, 1500, 1500, secs * 1000, secs * 1000);
  HINTERNET con = WinHttpConnect(ses, whost.c_str(), INTERNET_DEFAULT_PORT, 0);
  if (!con) {
    WinHttpCloseHandle(ses);
    return {};
  }
  DWORD flags = secure ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET req = WinHttpOpenRequest(con, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if (!req) {
    WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return {};
  }
  BOOL ok = WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
  if (ok) ok = WinHttpReceiveResponse(req, nullptr);
  if (ok) {
    DWORD avail = 0;
    while (WinHttpQueryDataAvailable(req, &avail) && avail) {
      if (body.size() > 256 * 1024) break;
      std::string chunk(avail, '\0');
      DWORD read = 0;
      if (!WinHttpReadData(req, chunk.data(), avail, &read)) break;
      chunk.resize(read);
      body += chunk;
    }
  }
  WinHttpCloseHandle(req);
  WinHttpCloseHandle(con);
  WinHttpCloseHandle(ses);
  return body;
#else
  int secs = timeout_ms / 1000;
  if (secs < 1) secs = 1;
  // Quote URL for sh: wrap in single quotes, escape embedded quotes.
  std::string q;
  for (char c : url) {
    if (c == '\'') q += "'\\''";
    else q.push_back(c);
  }
  char cmd[3072];
  std::snprintf(cmd, sizeof(cmd),
                "curl -fsS --max-time %d -A Wilfred/1.0 '%s' 2>/dev/null", secs, q.c_str());
  FILE* f = popen(cmd, "r");
  if (!f) return {};
  std::string body;
  char buf[2048];
  while (fgets(buf, sizeof(buf), f)) {
    body += buf;
    if (body.size() > 256 * 1024) break;
  }
  pclose(f);
  return body;
#endif
}

std::vector<SearchResult> RemoteProvider::query(const std::string& text, const Config& cfg,
                                                std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.remotes.enabled || limit == 0) return out;
  if (cfg.remotes.sources.empty()) return out;
  auto needle = normalize_query(text);
  if (needle.size() < 2) return out;
  int budget = cfg.remotes.max_results;
  if (budget < 1) budget = 1;
  if (budget > 50) budget = 50;
  std::size_t per = static_cast<std::size_t>(budget);
  for (auto& s : cfg.remotes.sources) {
    if (out.size() >= static_cast<std::size_t>(budget)) break;
    if (s.url.empty()) continue;
    auto url = remote_url_for(s.url, text);
    auto body = remote_fetch(url, cfg.remotes.timeout_ms);
    if (body.empty()) continue;
    auto part = remote_parse_response(body, s.name, text);
    for (auto& r : part) {
      if (out.size() >= static_cast<std::size_t>(budget) || out.size() >= limit) break;
      // Tag the source when the name is set.
      if (!s.name.empty() && r.subtitle.find(s.name) == std::string::npos)
        r.subtitle += (r.subtitle.empty() ? "" : " · ") + s.name;
      out.push_back(std::move(r));
    }
    (void)per;
  }
  return out;
}

}  // namespace wilfred
