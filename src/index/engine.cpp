#include "wilfred/index/engine.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/thread_pool.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/fs/classify.hpp"
#include "wilfred/fs/walker.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/content.hpp"

#include <chrono>
#include <filesystem>

namespace wilfred {
namespace fs = std::filesystem;

IndexEngine::IndexEngine() = default;
IndexEngine::~IndexEngine() { close(); }

bool IndexEngine::open(const std::string& dir, const Config& cfg) {
  cfg_ = cfg;
  dir_ = dir;
  create_directories(dir_);
  snapshot_path_ = path_join(dir_, "snapshot.wilf");
  wal_path_ = path_join(dir_, "journal.wal");
  cancel_ = false;
  bool snapshot_ok = false;
  if (file_exists(snapshot_path_)) {
    if (!store_.load(snapshot_path_)) {
      log_warn("index", "snapshot corrupt or unreadable; attempting WAL recovery");
      store_.clear();
    } else {
      snapshot_ok = true;
    }
  }
  if (!wal_.open(wal_path_)) {
    log_error("index", "unable to open write-ahead log");
    return false;
  }
  recover(snapshot_ok);
  {
    std::lock_guard<std::mutex> lock(stats_mu_);
    stats_.files = 0;
    stats_.dirs = 0;
    stats_.apps = 0;
    for (std::uint32_t i = 1; i < store_.records().size(); ++i) {
      auto* r = store_.get(i);
      if (!r) continue;
      if (has_flag(r->flags, RecordFlags::Directory))
        ++stats_.dirs;
      else
        ++stats_.files;
      if (r->kind == FileKind::Application) ++stats_.apps;
    }
  }
  log_info("index", "opened index with existing records");
  return true;
}

void IndexEngine::close() {
  checkpoint();
  wal_.close();
}

bool IndexEngine::recover(bool snapshot_loaded) {
  std::vector<WalEntry> entries;
  if (!wal_.replay(entries)) return false;
  std::size_t start = 0;
  if (snapshot_loaded) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
      if (entries[i].op == WalOp::Checkpoint) start = i + 1;
    }
  }
  std::size_t applied = 0;
  for (std::size_t i = start; i < entries.size(); ++i) {
    auto& e = entries[i];
    try {
      if (e.op == WalOp::Upsert)
        store_.upsert(e.record, e.path);
      else if (e.op == WalOp::Delete)
        store_.remove_path(e.path);
      else if (e.op == WalOp::Rename)
        store_.rename_path(e.path, e.new_path);
      else if (e.op == WalOp::Checkpoint)
        continue;
      ++applied;
    } catch (...) {
      log_warn("index", "skipped malformed WAL entry during recovery");
    }
  }
  if (applied) log_info("index", "replayed write-ahead log entries");
  return true;
}

bool IndexEngine::persist_snapshot() {
  if (!store_.save(snapshot_path_)) {
    log_error("index", "failed to write snapshot");
    return false;
  }
  wal_.append_checkpoint();
  wal_.truncate();
  dirty_ = 0;
  return true;
}

void IndexEngine::checkpoint() {
  if (dirty_ == 0 && wal_.size_bytes() == 0) return;
  persist_snapshot();
}

void IndexEngine::compact() {
  store_.rebuild_secondary();
  persist_snapshot();
}

static IndexRecord record_from_stat(const std::string& path, const FileStat& st, const Config& cfg) {
  IndexRecord rec;
  rec.size = st.size;
  rec.ctime = st.ctime;
  rec.mtime = st.mtime;
  rec.atime = st.atime;
  rec.mode = st.mode;
  rec.kind = classify_path(path, st.is_dir, st.is_exec);
  auto flags = RecordFlags::None;
  if (st.is_dir) flags = flags | RecordFlags::Directory;
  if (st.is_symlink) flags = flags | RecordFlags::Symlink;
  if (st.is_hidden) flags = flags | RecordFlags::Hidden;
  if (st.is_system || is_system_path(cfg, path)) flags = flags | RecordFlags::System;
  if (st.is_exec) flags = flags | RecordFlags::Executable;
  if (rec.kind == FileKind::Application) flags = flags | RecordFlags::Application;
  rec.flags = flags;
  return rec;
}

bool IndexEngine::upsert_file(const std::string& path) {
  try {
    auto st = stat_path(path);
    if (!st.exists) {
      return remove_path(path);
    }
    auto name = path_filename(path);
    if (path_is_excluded(cfg_, path, name)) return false;
    bool sys = is_system_path(cfg_, path) || st.is_system;
    if (sys && !cfg_.index.index_system) return false;
    if (st.is_hidden && !cfg_.index.index_hidden) return false;
    if (!st.is_dir && !extension_allowed(cfg_, path_extension(path))) return false;
    if (!st.is_dir && cfg_.index.max_file_size_bytes > 0 &&
        st.size > cfg_.index.max_file_size_bytes)
      return false;
    auto rec = record_from_stat(path, st, cfg_);
    const bool want_content = cfg_.index.content_indexing && !st.is_dir &&
                              content_indexable(rec.kind, path) &&
                              st.size <= cfg_.index.content_max_bytes;
    if (const IndexRecord* old = store_.by_path(path)) {
      if (old->size == rec.size && old->mtime == rec.mtime && old->ctime == rec.ctime &&
          old->kind == rec.kind && old->flags == rec.flags &&
          (!want_content || store_.has_content_tokens(old->id)))
        return true;
    }
    bool existed = store_.by_path(path) != nullptr;
    auto id = store_.upsert(rec, path);
    if (want_content) {
      std::string text;
      if (read_file_all(path, text) && !looks_binary(text)) {
        if (text.size() > cfg_.index.content_max_bytes)
          text.resize(static_cast<std::size_t>(cfg_.index.content_max_bytes));
        store_.add_content_tokens(id, extract_content_tokens(text, cfg_.index.content_max_tokens));
      }
    }
    wal_.append_upsert(rec, path);
    ++dirty_;
    generation_.fetch_add(1);
    if (cfg_.index.persist_every_records > 0 &&
        dirty_ >= static_cast<std::uint64_t>(cfg_.index.persist_every_records))
      persist_snapshot();
    if (wal_.size_bytes() >= cfg_.index.wal_compact_bytes) persist_snapshot();
    if (!existed) {
      std::lock_guard<std::mutex> lock(stats_mu_);
      if (st.is_dir)
        ++stats_.dirs;
      else
        ++stats_.files;
    }
    return true;
  } catch (const std::exception& ex) {
    log_warn("index", std::string("upsert failed: ") + ex.what());
    std::lock_guard<std::mutex> lock(stats_mu_);
    ++stats_.errors;
    return false;
  } catch (...) {
    std::lock_guard<std::mutex> lock(stats_mu_);
    ++stats_.errors;
    return false;
  }
}

bool IndexEngine::remove_path(const std::string& path) {
  bool ok = store_.remove_path(path);
  if (ok) {
    wal_.append_delete(path);
    ++dirty_;
    generation_.fetch_add(1);
  }
  return ok;
}

bool IndexEngine::rename_path(const std::string& from, const std::string& to) {
  bool ok = store_.rename_path(from, to);
  if (ok) {
    wal_.append_rename(from, to);
    ++dirty_;
    generation_.fetch_add(1);
  } else {
    remove_path(from);
    upsert_file(to);
  }
  return true;
}

void IndexEngine::scan_roots(const std::vector<std::string>& extra) {
  auto t0 = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(stats_mu_);
    stats_.scanning = true;
  }
  std::vector<std::string> roots = cfg_.index.paths;
  if (roots.empty()) roots = default_index_roots();
  roots.insert(roots.end(), extra.begin(), extra.end());

  std::size_t workers = cfg_.index.workers > 0
                            ? static_cast<std::size_t>(cfg_.index.workers)
                            : adaptive_index_workers(cfg_.index.cpu_percent_limit,
                                                     static_cast<std::size_t>(cfg_.index.memory_limit_mb));
  ThreadPool pool(workers);
  std::atomic<std::uint64_t> seen{0};
  const std::size_t bound = std::max<std::size_t>(
      64, static_cast<std::size_t>(std::max(1, cfg_.index.batch_size)) * 4);

  for (auto& root : roots) {
    if (cancel_) break;
    std::error_code ec;
    if (!fs::exists(fs::u8path(root), ec)) {
      log_warn("index", "index root does not exist: " + root);
      continue;
    }
    WalkStats ws;
    walk_tree(
        root, cfg_,
        [&](const WalkEntry& e) {
          if (cancel_) return;
          pool.post_bounded([this, path = e.path] { upsert_file(path); }, bound);
          auto n = seen.fetch_add(1) + 1;
          if (n % 5000 == 0 && progress_) {
            auto st = stats();
            st.files = n;
            progress_(st);
          }
        },
        &cancel_, &ws);
    std::lock_guard<std::mutex> lock(stats_mu_);
    stats_.errors += ws.errors;
  }
  pool.wait_idle();
  try {
    index_applications(*this);
  } catch (...) {
    log_warn("index", "application discovery failed");
  }
  persist_snapshot();
  auto t1 = std::chrono::steady_clock::now();
  {
    std::lock_guard<std::mutex> lock(stats_mu_);
    stats_.scanning = false;
    stats_.last_scan_seconds =
        std::chrono::duration<double>(t1 - t0).count();
    stats_.files = store_.live_count();
  }
  log_info("index", "scan complete");
}

IndexStats IndexEngine::stats() const {
  std::lock_guard<std::mutex> lock(stats_mu_);
  auto s = stats_;
  s.files = store_.live_count();
  return s;
}

}  // namespace wilfred
