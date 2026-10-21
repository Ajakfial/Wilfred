#include "wilfred/sources/menu.hpp"

#include "wilfred/platform/native.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {
namespace {

std::mutex g_mu;
std::vector<NativeMenuItem> g_items;
std::chrono::steady_clock::time_point g_fetched{};
bool g_fetched_once{false};

// Menu trees change when the user switches apps or opens a dialog. A short TTL
// keeps the results plausible without paying the enumeration cost per keystroke.
constexpr int kTtlSecs = 10;

std::string lower_copy(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (unsigned char c : s) o.push_back(static_cast<char>(std::tolower(c)));
  return o;
}

std::string trim_ws(const std::string& s) {
  std::size_t b = 0;
  std::size_t e = s.size();
  while (b < e && (s[b] == ' ' || s[b] == '\t')) ++b;
  while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t')) --e;
  return s.substr(b, e - b);
}

}  // namespace

std::vector<SearchResult> MenuProvider::query(const std::string& text, const Config& cfg,
                                              std::size_t limit) {
  std::vector<SearchResult> out;
  if (!cfg.sources.menu) return out;
  const std::string q = lower_copy(trim_ws(text));
  // One character matches too much to be worth a process-wide enumeration.
  if (q.size() < 2) return out;

  bool fresh = false;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    if (g_fetched_once) {
      const auto age = std::chrono::steady_clock::now() - g_fetched;
      fresh = age < std::chrono::seconds(kTtlSecs);
    }
  }
  if (!fresh) {
    std::vector<NativeMenuItem> items;
    std::string err;
    if (!native_active_menu_items(items, err)) return out;
    {
      std::lock_guard<std::mutex> lock(g_mu);
      g_items = std::move(items);
      g_fetched = std::chrono::steady_clock::now();
      g_fetched_once = true;
    }
  }

  std::vector<NativeMenuItem> items;
  {
    std::lock_guard<std::mutex> lock(g_mu);
    items = g_items;
  }

  for (const auto& it : items) {
    if (out.size() >= limit) break;
    const std::string hay = lower_copy(it.path + " " + it.title);
    if (hay.find(q) == std::string::npos) continue;
    SearchResult r;
    r.title = it.title;
    r.subtitle = it.app.empty() ? it.path : it.path + " · " + it.app;
    if (!it.shortcut.empty()) r.subtitle += " (" + it.shortcut + ")";
    r.path = "menu:" + it.id;
    r.payload = r.path;
    r.action = ResultAction::Open;
    r.score = 8500;
    r.kind_label = "menu";
    r.category = "menu";
    out.push_back(std::move(r));
  }
  return out;
}

void menu_forget_cache() {
  std::lock_guard<std::mutex> lock(g_mu);
  g_items.clear();
  g_fetched_once = false;
}

}  // namespace wilfred
