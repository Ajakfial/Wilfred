#include "wilfred/service/service.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/volumes.hpp"
#include "wilfred/fs/watcher.hpp"
#include "wilfred/hotkey/hotkey.hpp"
#include "wilfred/ipc/server.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/ui/overlay.hpp"

#include <chrono>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <csignal>
#endif

namespace wilfred {
namespace {

std::atomic<bool>* g_running = nullptr;

#ifdef _WIN32
BOOL WINAPI console_handler(DWORD type) {
  if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
    if (g_running) g_running->store(false);
    return TRUE;
  }
  return FALSE;
}
#else
void on_signal(int) {
  if (g_running) g_running->store(false);
}
#endif

LogLevel parse_level(const std::string& s) {
  auto l = to_lower_utf8(s);
  if (l == "error") return LogLevel::Error;
  if (l == "warn") return LogLevel::Warn;
  if (l == "debug") return LogLevel::Debug;
  return LogLevel::Info;
}

}  // namespace

Service::Service() : search_(index_), interpreter_(index_, search_) {}

Service::~Service() {
  running_ = false;
  if (hotkey_) hotkey_->stop();
  if (watcher_) watcher_->stop();
  if (ipc_) ipc_->stop();
  if (ui_) ui_->destroy();
  history_.save(default_history_path());
  index_.close();
}

void Service::apply_logging() {
  Logger::instance().set_level(parse_level(cfg_.logging.level));
  auto path = cfg_.logging.file.empty() ? default_log_path() : cfg_.logging.file;
  Logger::instance().set_file(path, cfg_.logging.max_file_bytes);
}

bool Service::boot() {
  ConfigError err;
  cfg_ = load_or_create_user_config(err);
  if (!err.message.empty()) log_warn("config", err.message);
  apply_logging();
  create_directories(data_directory());
  if (!index_.open(default_index_path(), cfg_)) {
    log_error("service", "failed to open index");
    return false;
  }
  history_.set_enabled(cfg_.history.enabled);
  history_.set_max(cfg_.history.max_entries);
  if (cfg_.history.persist) history_.load(default_history_path());
  return true;
}

void Service::on_hotkey() {
  if (ui_) {
    if (ui_->visible())
      ui_->hide();
    else
      ui_->show();
  }
}

int Service::run_search(const std::string& query, int limit) {
  if (!boot()) return 1;
  if (index_.store().live_count() == 0) {
    log_info("service", "index is empty; scanning once");
    index_.scan_roots();
  }
  history_.record_query(query);
  auto iq = interpreter_.interpret(query, cfg_, &history_);
  int n = 0;
  for (auto& r : iq.results) {
    if (n >= limit) break;
    std::cout << r.score << '\t' << r.title << '\t' << r.path << '\n';
    ++n;
  }
  if (cfg_.history.persist) history_.save(default_history_path());
  return 0;
}

int Service::launch_by_query(const std::string& query) {
  if (!boot()) return 1;
  auto iq = interpreter_.interpret(query, cfg_, &history_);
  const SearchResult* pick = nullptr;
  for (auto& r : iq.results) {
    if (r.action != ResultAction::Habit) {
      pick = &r;
      break;
    }
  }
  if (!pick) {
    std::cerr << "no results\n";
    return 1;
  }
  history_.record_query(query);
  auto key = pick->path.empty() ? pick->payload : pick->path;
  if (!key.empty() && result_is_launchable(*pick)) {
    history_.record_selection(key);
    history_.record_choice(query, key);
  }
  bool ok = execute_result(*pick, cfg_);
  if (cfg_.history.persist) history_.save(default_history_path());
  return ok ? 0 : 1;
}

int Service::run_index_now() {
  if (!boot()) return 1;
  index_.scan_roots();
  auto st = index_.stats();
  std::cout << "indexed " << st.files << " entries in " << st.last_scan_seconds << "s ("
            << st.errors << " errors)\n";
  return 0;
}

int Service::run_status() {
  if (!boot()) return 1;
  auto st = index_.stats();
  std::cout << "platform: " << platform_name() << "\n"
            << "config: " << cfg_.source_path << "\n"
            << "index: " << default_index_path() << "\n"
            << "records: " << st.files << "\n"
            << "dirs: " << st.dirs << "\n"
            << "apps: " << st.apps << "\n"
            << "errors: " << st.errors << "\n"
            << "scanning: " << (st.scanning ? "yes" : "no") << "\n"
            << "last scan (s): " << st.last_scan_seconds << "\n";
  return 0;
}

int Service::run_daemon() {
  if (!boot()) return 1;
  running_ = true;
  g_running = &running_;
#ifdef _WIN32
  SetConsoleCtrlHandler(console_handler, TRUE);
#else
  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
#endif

  watcher_ = std::make_unique<FsWatcher>();
  hotkey_ = std::make_unique<GlobalHotkey>();
  ipc_ = std::make_unique<IpcServer>();
  ui_ = create_overlay();

  overlay_bind(
      [this](const std::string& q) {
        last_overlay_query_ = q;
        auto iq = interpreter_.interpret(q, cfg_, &history_);
        return iq.results;
      },
      [this](const SearchResult& r) {
        if (r.action == ResultAction::Habit) return;
        if (r.category == "mini" || r.category == "macro" || r.category == "clipboard" ||
            r.action == ResultAction::Copy || r.action == ResultAction::Mini) {
          execute_result(r, cfg_);
          if (ui_) ui_->hide();
          return;
        }
        if (!last_overlay_query_.empty()) history_.record_query(last_overlay_query_);
        auto key = r.path.empty() ? r.payload : r.path;
        if (!key.empty() && result_is_launchable(r)) {
          history_.record_selection(key);
          history_.record_choice(last_overlay_query_, key);
        }
        execute_result(r, cfg_);
        if (ui_) ui_->hide();
        if (cfg_.history.persist) history_.save(default_history_path());
      });
  overlay_set_quit([this] { running_ = false; });
  if (ui_) ui_->create();

  auto roots = cfg_.index.paths;
  if (roots.empty()) roots = default_index_roots();
  watcher_->start(roots, cfg_.index.debounce_fs_ms, [this](const FsEvent& ev) {
    try {
      if (ev.kind == FsEventKind::Deleted)
        index_.remove_path(ev.path);
      else if (ev.kind == FsEventKind::Renamed && !ev.new_path.empty())
        index_.rename_path(ev.path, ev.new_path);
      else if (ev.kind == FsEventKind::Overflow)
        index_.scan_roots();
      else
        index_.upsert_file(ev.path);
    } catch (...) {
      log_warn("watch", "filesystem event handling failed");
    }
  });

  if (cfg_.hotkey.enabled) {
    if (!hotkey_->start(cfg_, [this] { on_hotkey(); }))
      log_warn("hotkey", "failed to register global hotkey");
  }

  ipc_->start(ipc_endpoint(), [this](const IpcRequest& req) {
    IpcResponse resp;
    try {
      if (req.cmd == "search") {
        auto iq = interpreter_.interpret(req.query, cfg_, &history_);
        if (req.limit > 0 && static_cast<int>(iq.results.size()) > req.limit)
          iq.results.resize(static_cast<std::size_t>(req.limit));
        resp.results = std::move(iq.results);
      } else if (req.cmd == "launch") {
        resp.ok = launch_path(req.path);
      } else if (req.cmd == "status") {
        auto st = index_.stats();
        resp.text = std::to_string(st.files);
      } else if (req.cmd == "index") {
        index_.scan_roots();
        resp.text = "ok";
      } else if (req.cmd == "show") {
        on_hotkey();
      } else {
        resp.ok = false;
        resp.error = "unknown command";
      }
    } catch (const std::exception& ex) {
      resp.ok = false;
      resp.error = ex.what();
    }
    return resp;
  });

  std::thread indexer([this] {
    try {
      if (index_.store().live_count() == 0) index_.scan_roots();
    } catch (...) {
      log_warn("service", "initial scan failed");
    }
    auto last_vol_check = std::chrono::steady_clock::now();
    auto volumes = list_volumes();
    while (running_) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::seconds>(now - last_vol_check).count() >= 15) {
        last_vol_check = now;
        auto next = list_volumes();
        for (auto& v : next) {
          bool known = false;
          for (auto& o : volumes)
            if (o.path == v.path) known = true;
          if (!known && v.ready) {
            log_info("volumes", "mounted " + v.path);
            watcher_->add_root(v.path);
            index_.scan_roots({v.path});
          }
        }
        volumes = std::move(next);
      }
      if (cfg_.index.rescan_interval_seconds > 0) {
        static auto last = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last).count() >=
            cfg_.index.rescan_interval_seconds) {
          last = now;
          try {
            index_.scan_roots();
          } catch (...) {
          }
        }
      }
    }
  });

  log_info("service", "Wilfred daemon running");
  while (running_) {
#ifdef _WIN32
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 200, QS_ALLINPUT);
    pump_native_events();
#else
    pump_native_events();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
#endif
  }
  if (indexer.joinable()) indexer.join();
  index_.checkpoint();
  if (cfg_.history.persist) history_.save(default_history_path());
  return 0;
}

}  // namespace wilfred
