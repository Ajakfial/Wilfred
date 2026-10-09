#include "wilfred/search/disktools.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/file_ops.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <unordered_map>

namespace wilfred {
namespace {

std::string human_bytes_local(std::uint64_t n) {
  const char* u[] = {"B", "KB", "MB", "GB", "TB"};
  double v = static_cast<double>(n);
  int i = 0;
  while (v >= 1024.0 && i < 4) {
    v /= 1024.0;
    ++i;
  }
  char buf[64];
  if (i == 0)
    std::snprintf(buf, sizeof(buf), "%llu %s", (unsigned long long)n, u[i]);
  else
    std::snprintf(buf, sizeof(buf), "%.1f %s", v, u[i]);
  return buf;
}

SearchResult card_local(const std::string& title, const std::string& sub,
                        const std::string& payload, const std::string& label, int score) {
  SearchResult r;
  r.title = title;
  r.subtitle = sub;
  r.payload = payload;
  r.path = payload;
  r.action = ResultAction::Open;
  r.score = score;
  r.kind_label = label;
  r.category = label;
  return r;
}

bool under_filter(const std::string& path, const std::string& filter) {
  if (filter.empty()) return true;
  auto p = to_lower_utf8(path);
  auto f = to_lower_utf8(filter);
  return p.find(f) != std::string::npos;
}

}  // namespace

std::vector<SearchResult> large_file_results(IndexEngine& index, const Config& cfg,
                                             const std::string& dir_filter, int limit) {
  (void)cfg;
  if (limit <= 0) limit = 8;
  if (limit > 25) limit = 25;
  struct Item {
    std::string path;
    std::uint64_t size{0};
  };
  std::vector<Item> items;
  {
    auto& store = index.store();
    std::lock_guard<std::recursive_mutex> lock(store.mutex());
    for (auto& rec : store.records()) {
      if (rec.size == 0) continue;
      if (has_flag(rec.flags, RecordFlags::Directory)) continue;
      std::string_view pv = store.pool().get(rec.path_id);
      if (pv.empty()) continue;
      std::string path(pv);
      if (!under_filter(path, dir_filter)) continue;
      items.push_back({path, rec.size});
    }
  }
  std::sort(items.begin(), items.end(), [](auto& a, auto& b) { return a.size > b.size; });
  std::vector<SearchResult> out;
  int n = 0;
  for (auto& it : items) {
    if (n >= limit) break;
    std::string fn = path_filename(it.path);
    std::string title = fn + "  ·  " + human_bytes_local(it.size);
    auto r = card_local(title, it.path, it.path, "large", 10000 - n * 10);
    r.meter = -1;
    out.push_back(std::move(r));
    ++n;
  }
  if (out.empty()) {
    SearchResult r = card_local(
        "No files found", dir_filter.empty() ? "Index is empty" : dir_filter, "", "large", 9000);
    r.action = ResultAction::None;
    out.push_back(std::move(r));
  }
  return out;
}

std::vector<SearchResult> dupe_file_results(IndexEngine& index, const Config& cfg,
                                            const std::string& dir_filter, int limit) {
  (void)cfg;
  if (limit <= 0) limit = 8;
  if (limit > 25) limit = 25;
  // Group by size first.
  std::unordered_map<std::uint64_t, std::vector<std::string>> by_size;
  {
    auto& store = index.store();
    std::lock_guard<std::recursive_mutex> lock(store.mutex());
    for (auto& rec : store.records()) {
      if (rec.size == 0) continue;
      if (has_flag(rec.flags, RecordFlags::Directory)) continue;
      std::string_view pv = store.pool().get(rec.path_id);
      if (pv.empty()) continue;
      std::string path(pv);
      if (!under_filter(path, dir_filter)) continue;
      by_size[rec.size].push_back(path);
    }
  }
  struct Group {
    std::uint64_t size{0};
    std::vector<std::string> paths;
  };
  std::vector<Group> groups;
  for (auto& [sz, paths] : by_size) {
    if (paths.size() < 2) continue;
    // Verify with a cheap content hash for the first few candidates to avoid
    // false positives from size collisions. Cap work: max 4 groups, hash up
    // to 6 files each, skip files > 256MB.
    std::unordered_map<std::string, std::vector<std::string>> by_hash;
    int hashed = 0;
    for (auto& p : paths) {
      if (hashed >= 6) break;
      std::string hex, err;
      // Skip very large files for responsiveness.
      bool too_big = false;
      {
        auto& store = index.store();
        std::lock_guard<std::recursive_mutex> lock(store.mutex());
        if (auto* rec = store.by_path(p)) {
          if (rec->size > 256ull * 1024 * 1024) too_big = true;
        }
      }
      if (too_big) continue;
      if (!sha256_file(p, hex, err)) {
        // Unreadable: group by size alone under a synthetic key.
        by_hash["unreadable:" + std::to_string(sz)].push_back(p);
        continue;
      }
      by_hash[hex].push_back(p);
      ++hashed;
    }
    for (auto& [h, ps] : by_hash) {
      if (ps.size() < 2) continue;
      groups.push_back({sz, ps});
    }
    if (groups.size() >= 4) break;
  }
  std::sort(groups.begin(), groups.end(), [](auto& a, auto& b) { return a.size > b.size; });
  std::vector<SearchResult> out;
  int n = 0;
  for (auto& g : groups) {
    if (n >= limit) break;
    for (auto& p : g.paths) {
      if (n >= limit) break;
      std::string fn = path_filename(p);
      std::string title =
          fn + "  ·  " + human_bytes_local(g.size) + " ×" + std::to_string(g.paths.size());
      auto r = card_local(title, p, p, "dupe", 10000 - n * 5);
      out.push_back(std::move(r));
      ++n;
    }
  }
  if (out.empty()) {
    SearchResult r = card_local("No duplicates found",
                                dir_filter.empty() ? "No same-size files in index" : dir_filter, "",
                                "dupe", 9000);
    r.action = ResultAction::None;
    out.push_back(std::move(r));
  }
  return out;
}

}  // namespace wilfred
