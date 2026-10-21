#include "wilfred/sources/vault.hpp"

#include "wilfred/core/json.hpp"
#include "wilfred/search/clipboard.hpp"

#include <chrono>
#include <cctype>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace wilfred {
namespace {

struct VaultItem {
  std::string id;
  std::string title;
  std::string username;
};

std::mutex g_mu;
std::vector<VaultItem> g_items;
std::chrono::steady_clock::time_point g_fetched{};
bool g_fetched_once{false};

constexpr const char* kVaultScheme = "vault:";

std::string lower_copy(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (unsigned char c : s) o.push_back(static_cast<char>(std::tolower(c)));
  return o;
}

std::string trim_ws(const std::string& s) {
  std::size_t b = 0;
  std::size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
    --e;
  return s.substr(b, e - b);
}

// Single-quote so item ids and vault paths survive the shell. Ids come from
// our own JSON, but they still cross a command line.
std::string shell_quote(const std::string& s) {
#ifdef _WIN32
  std::string out = "\"";
  for (char c : s) {
    if (c == '"') out += "\\\"";
    out += c;
  }
  out += "\"";
  return out;
#else
  std::string out = "'";
  for (char c : s) {
    if (c == '\'')
      out += "'\\''";
    else
      out += c;
  }
  out += "'";
  return out;
#endif
}

// Runs a command and captures stdout. stderr is discarded so a failing CLI
// cannot leak anything into logs. Output is capped so a misbehaving binary
// cannot exhaust memory.
bool run_capture(const std::string& cmd, std::string& out) {
  out.clear();
#ifdef _WIN32
  FILE* f = _popen(cmd.c_str(), "r");
#else
  FILE* f = popen(cmd.c_str(), "r");
#endif
  if (!f) return false;
  char buf[4096];
  constexpr std::size_t kCap = 16u * 1024u * 1024u;
  while (std::fgets(buf, sizeof(buf), f)) {
    out += buf;
    if (out.size() > kCap) break;
  }
  if (out.size() > kCap) out.resize(kCap);
#ifdef _WIN32
  const int rc = _pclose(f);
#else
  const int rc = pclose(f);
#endif
  return rc == 0;
}

bool cache_fresh(int ttl_secs) {
  if (!g_fetched_once) return false;
  if (ttl_secs <= 0) return false;
  const auto age = std::chrono::steady_clock::now() - g_fetched;
  return age < std::chrono::seconds(ttl_secs);
}

bool fetch_items(const Config& cfg, std::string& error) {
  std::string raw;
  const std::string cmd = cfg.sources.vault_command + " item list --format=json";
  if (!run_capture(cmd, raw)) {
    error = "password manager CLI failed (is it installed and signed in?)";
    return false;
  }
  std::vector<VaultItem> items;
  for (const auto& obj : json_object_array(raw)) {
    VaultItem it;
    it.id = json_get_string(obj, "id");
    it.title = json_get_string(obj, "title");
    it.username = json_get_string(obj, "additional_information");
    if (it.id.empty()) continue;
    if (it.title.empty()) it.title = it.username.empty() ? it.id : it.username;
    items.push_back(std::move(it));
  }
  {
    std::lock_guard<std::mutex> lock(g_mu);
    g_items = std::move(items);
    g_fetched = std::chrono::steady_clock::now();
    g_fetched_once = true;
  }
  return true;
}

// Best-effort zeroization. Not a guarantee on optimizing compilers, but it
// keeps the secret out of freed heap blocks in practice.
void wipe(std::string& s) {
  if (!s.empty()) {
    volatile char* p = &s[0];
    for (std::size_t i = 0; i < s.size(); ++i) p[i] = 0;
  }
  s.clear();
}

}  // namespace

std::vector<SearchResult> VaultProvider::query(const std::string& text, const Config& cfg,
                                               std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.sources.vault) return out;
  const std::string q = lower_copy(trim_ws(text));
  if (q.empty()) return out;

  bool fresh = false;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    fresh = cache_fresh(cfg.sources.vault_cache_secs);
  }
  if (!fresh) {
    std::string err;
    if (!fetch_items(cfg, err)) return out;
  }

  std::vector<VaultItem> items;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    items = g_items;
  }

  for (const auto& it : items) {
    if (out.size() >= limit) break;
    const std::string hay = lower_copy(it.title + " " + it.username);
    if (hay.find(q) == std::string::npos) continue;
    SearchResult r;
    r.title = it.title;
    r.subtitle = it.username.empty() ? "Password · Enter copies" : it.username + " · Password";
    // Only the opaque id crosses into the result. The secret is fetched later.
    r.path = std::string(kVaultScheme) + it.id;
    r.payload = r.path;
    r.action = ResultAction::Copy;
    r.score = 9000;
    r.kind_label = "password";
    r.category = "vault";
    out.push_back(std::move(r));
  }
  return out;
}

bool vault_copy_secret_to_clipboard(const Config& cfg, const std::string& item_ref,
                                    std::string& error) {
  error.clear();
  std::string id = item_ref;
  const std::string scheme(kVaultScheme);
  if (id.rfind(scheme, 0) == 0) id = id.substr(scheme.size());
  if (id.empty()) {
    error = "missing vault item id";
    return false;
  }

  const std::string cmd = cfg.sources.vault_command + " item get " + shell_quote(id) +
                          " --fields password --reveal";
  std::string plain;
  if (!run_capture(cmd, plain)) {
    wipe(plain);
    error = "could not read the item (is the CLI unlocked?)";
    return false;
  }
  plain = trim_ws(plain);
  if (plain.empty()) {
    error = "the item has no password field";
    return false;
  }

  if (!write_clipboard(plain)) {
    wipe(plain);
    error = "could not write to the clipboard";
    return false;
  }

  // Clear the clipboard later, but only if nobody has copied something else
  // in the meantime -- clobbering a fresh copy would be worse than leaving it.
  // The comparison marker is read back from the clipboard so platform line-
  // ending normalization does not defeat the check.
  const int secs = cfg.sources.vault_clear_after_secs;
  if (secs > 0) {
    struct Held {
      std::string secret;
      std::string marker;
    };
    auto held = std::make_shared<Held>();
    held->secret = plain;
    held->marker = read_clipboard().text;
    if (held->marker.empty()) held->marker = plain;
    wipe(plain);
    std::thread([held, secs] {
      std::this_thread::sleep_for(std::chrono::seconds(secs));
      const auto snap = read_clipboard();
      if (snap.text == held->marker) write_clipboard("");
      wipe(held->secret);
      wipe(held->marker);
    }).detach();
  } else {
    wipe(plain);
  }
  return true;
}

void vault_forget_cache() {
  std::lock_guard<std::mutex> lock(g_mu);
  g_items.clear();
  g_fetched_once = false;
}

}  // namespace wilfred
