#include "wilfred/fs/walker.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/fs/classify.hpp"

#include <filesystem>
#include <stack>

namespace wilfred {
namespace fs = std::filesystem;

void walk_tree(const std::string& root, const Config& cfg, WalkFn on_entry,
               std::atomic<bool>* cancel, WalkStats* stats) {
  std::error_code ec;
  auto start = fs::u8path(root);
  if (!fs::exists(start, ec)) {
    if (stats) ++stats->errors;
    return;
  }
  std::stack<fs::path> dirs;
  dirs.push(start);

  auto emit = [&](const fs::path& p) {
    WalkEntry e;
    auto u8 = p.u8string();
    e.path = std::string(u8.begin(), u8.end());
    e.st = stat_path(e.path);
    try {
      on_entry(e);
    } catch (...) {
      if (stats) ++stats->errors;
    }
    if (stats) ++stats->visited;
  };

  emit(start);

  while (!dirs.empty()) {
    if (cancel && cancel->load()) return;
    auto dir = dirs.top();
    dirs.pop();
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
      log_debug("walk", std::string("skip unreadable directory"));
      if (stats) ++stats->errors;
      continue;
    }
    fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
      if (ec) {
        if (stats) ++stats->errors;
        ec.clear();
        continue;
      }
      if (cancel && cancel->load()) return;
      const auto& entry = *it;
      auto name_u8 = entry.path().filename().u8string();
      std::string name(name_u8.begin(), name_u8.end());
      auto path_u8 = entry.path().u8string();
      std::string path(path_u8.begin(), path_u8.end());

      if (path_is_excluded(cfg, path, name)) {
        if (stats) ++stats->skipped;
        continue;
      }
      bool is_dir = false;
      bool is_sym = false;
      {
        auto lst = entry.symlink_status(ec);
        if (ec) {
          if (stats) ++stats->errors;
          continue;
        }
        is_sym = fs::is_symlink(lst);
        is_dir = fs::is_directory(lst);
        if (is_sym && cfg.index.follow_symlinks) {
          auto st = entry.status(ec);
          if (!ec) is_dir = fs::is_directory(st);
        }
      }
      if (is_sym && !cfg.index.follow_symlinks && is_dir) {
        emit(entry.path());
        if (stats) ++stats->skipped;
        continue;
      }
      bool sys = is_system_path(cfg, path);
      if (sys && !cfg.index.index_system) {
        if (stats) ++stats->skipped;
        continue;
      }
      bool hidden = looks_hidden(name, path);
#ifdef _WIN32
      // attributes filled in emit/stat
#endif
      if (hidden && !cfg.index.index_hidden) {
        if (stats) ++stats->skipped;
        continue;
      }
      emit(entry.path());
      if (is_dir) dirs.push(entry.path());
    }
  }
}

}  // namespace wilfred
