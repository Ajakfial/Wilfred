#include "wilfred/service/service.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/core/json.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/fs/volumes.hpp"
#include "wilfred/fs/watcher.hpp"
#include "wilfred/hotkey/hotkey.hpp"
#include "wilfred/ipc/http.hpp"
#include "wilfred/ipc/server.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/sync/backup.hpp"
#include "wilfred/ui/overlay.hpp"

#include <chrono>
#include <iostream>
#include <sstream>
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

std::string query_param(const std::string& qs, const char* key) {
  std::string prefix = std::string(key) + "=";
  std::size_t pos = 0;
  while (pos < qs.size()) {
    auto amp = qs.find('&', pos);
    auto part = qs.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
    if (part.rfind(prefix, 0) == 0) return part.substr(prefix.size());
    if (amp == std::string::npos) break;
    pos = amp + 1;
  }
  return {};
}

std::string json_ok(bool ok, const std::string& extra = {}) {
  std::string b = std::string("{\"ok\":") + (ok ? "true" : "false");
  if (!extra.empty()) b += "," + extra;
  b += "}";
  return b;
}

}  // namespace

Service::Service() : search_(index_), interpreter_(index_, search_, &snippets_, &plugins_) {}

Service::~Service() {
  running_ = false;
  if (hotkey_) hotkey_->stop();
  if (watcher_) watcher_->stop();
  if (http_) http_->stop();
  if (ipc_) ipc_->stop();
  if (ui_) ui_->destroy();
  set_plugin_host_for_actions(nullptr);
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
  snippets_.load(default_snippets_path(), cfg_);
  for (auto& d : default_plugin_directories()) create_directories(d);
  plugins_.load(cfg_);
  set_plugin_host_for_actions(&plugins_);
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
  std::cout << "plugins: " << plugins_.manifests().size() << "\n"
            << "snippets: " << snippets_.all().size() << "\n"
            << "api: " << (cfg_.api.enabled ? "on" : "off") << "\n";
  return 0;
}

int Service::run_backup(const std::string& dest, bool include_index) {
  if (!boot()) return 1;
  std::string err;
  auto path = dest.empty() ? default_backup_path() : dest;
  if (!create_backup(cfg_, path, include_index, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  std::cout << "backup written to " << path << "\n";
  return 0;
}

int Service::run_restore(const std::string& src) {
  ConfigError err;
  cfg_ = load_or_create_user_config(err);
  apply_logging();
  std::string e;
  auto path = src.empty() ? default_backup_path() : src;
  if (!restore_backup(path, e)) {
    std::cerr << e << "\n";
    return 1;
  }
  std::cout << "restored from " << path << "\n";
  return 0;
}

int Service::run_sync(bool push) {
  if (!boot()) return 1;
  std::string err;
  bool ok = push ? sync_push(cfg_, err) : sync_pull(cfg_, err);
  if (!ok) {
    std::cerr << err << "\n";
    return 1;
  }
  std::cout << (push ? "sync push ok\n" : "sync pull ok\n");
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
      [this](const SearchResult& r, const std::string& action_id) {
        if (r.action == ResultAction::Habit) return;
        auto hide = action_hides_overlay(action_id);
        if (r.category == "mini" || r.category == "macro" || r.category == "clipboard" ||
            r.action == ResultAction::Copy || r.action == ResultAction::Mini) {
          execute_result(r, cfg_, action_id);
          if (hide && ui_) ui_->hide();
          return;
        }
        if (!last_overlay_query_.empty()) history_.record_query(last_overlay_query_);
        auto key = r.path.empty() ? r.payload : r.path;
        if (!key.empty() && result_is_launchable(r)) {
          history_.record_selection(key);
          history_.record_choice(last_overlay_query_, key);
        }
        execute_result(r, cfg_, action_id);
        bool paste = action_id == "paste" || action_id == "expand" ||
                     (action_id.empty() && r.action == ResultAction::Expand);
        if (paste && !cfg_.snippets.auto_paste) {
          std::thread([] {
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            native_simulate_paste();
          }).detach();
        }
        if (hide && ui_) ui_->hide();
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
        if (!req.query.empty()) {
          auto iq = interpreter_.interpret(req.query, cfg_, &history_);
          if (iq.results.empty()) {
            resp.ok = false;
            resp.error = "no results";
          } else {
            resp.ok = execute_result(iq.results.front(), cfg_, req.action);
          }
        } else {
          SearchResult r;
          r.path = req.path;
          r.payload = req.path;
          resp.ok = execute_result(r, cfg_, req.action);
        }
      } else if (req.cmd == "exec" || req.cmd == "action") {
        SearchResult r;
        r.path = req.path;
        r.payload = req.path;
        r.title = req.query;
        resp.ok = execute_result(r, cfg_, req.action);
      } else if (req.cmd == "status") {
        auto st = index_.stats();
        resp.text = std::to_string(st.files);
      } else if (req.cmd == "index") {
        index_.scan_roots();
        resp.text = "ok";
      } else if (req.cmd == "show") {
        on_hotkey();
      } else if (req.cmd == "backup") {
        std::string err;
        auto dest = req.path.empty() ? default_backup_path() : req.path;
        resp.ok = create_backup(cfg_, dest, cfg_.sync.include_index, err);
        resp.text = dest;
        resp.error = err;
      } else if (req.cmd == "snippets") {
        std::ostringstream os;
        os << "[";
        bool first = true;
        for (auto& s : snippets_.all()) {
          if (!first) os << ',';
          first = false;
          os << s.trigger;
        }
        os << "]";
        resp.text = os.str();
      } else if (req.cmd == "restore") {
        std::string err;
        auto src = req.path.empty() ? default_backup_path() : req.path;
        resp.ok = restore_backup(src, err);
        resp.error = err;
      } else if (req.cmd == "sync-push") {
        std::string err;
        resp.ok = sync_push(cfg_, err);
        resp.error = err;
      } else if (req.cmd == "sync-pull") {
        std::string err;
        resp.ok = sync_pull(cfg_, err);
        resp.error = err;
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

  if (cfg_.api.enabled) {
    http_ = std::make_unique<HttpApiServer>();
    if (!http_->start(cfg_.api.bind, cfg_.api.port, cfg_.api.token, [this](const HttpApiRequest& req) {
      HttpApiResponse out;
      if (req.method == "OPTIONS") {
        out.body = "";
        return out;
      }
      auto path = req.path;
      if (path.rfind("/v1/", 0) == 0) path = path.substr(3);
      auto q = req.query;
      auto body = req.body;
      auto qtext = query_param(q, "q");
      if (qtext.empty()) qtext = json_get_string(body, "q");
      if (qtext.empty()) qtext = json_get_string(body, "query");
      auto action = query_param(q, "action");
      if (action.empty()) action = json_get_string(body, "action");
      auto rpath = query_param(q, "path");
      if (rpath.empty()) rpath = json_get_string(body, "path");
      int limit = json_get_int(body, "limit", 40);
      auto n = query_param(q, "limit");
      if (!n.empty()) {
        try {
          limit = std::stoi(n);
        } catch (...) {
        }
      }

      if ((path == "/search" || path == "/query") && (req.method == "GET" || req.method == "POST")) {
        auto iq = interpreter_.interpret(qtext, cfg_, &history_);
        if (limit > 0 && static_cast<int>(iq.results.size()) > limit)
          iq.results.resize(static_cast<std::size_t>(limit));
        IpcResponse ir;
        ir.results = std::move(iq.results);
        out.body = encode_response(ir);
        return out;
      }
      if (path == "/status" && req.method == "GET") {
        auto st = index_.stats();
        out.body = json_ok(true, "\"files\":" + std::to_string(st.files) + ",\"dirs\":" +
                                     std::to_string(st.dirs) + ",\"apps\":" + std::to_string(st.apps) +
                                     ",\"plugins\":" + std::to_string(plugins_.manifests().size()) +
                                     ",\"snippets\":" + std::to_string(snippets_.all().size()));
        return out;
      }
      if (path == "/show" && req.method == "POST") {
        on_hotkey();
        out.body = json_ok(true);
        return out;
      }
      if (path == "/index" && req.method == "POST") {
        index_.scan_roots();
        out.body = json_ok(true);
        return out;
      }
      if ((path == "/launch" || path == "/open" || path == "/exec") && req.method == "POST") {
        SearchResult r;
        r.path = rpath;
        r.payload = json_get_string(body, "payload");
        if (r.payload.empty()) r.payload = rpath;
        r.title = json_get_string(body, "title");
        r.plugin_id = json_get_string(body, "plugin");
        if (r.plugin_id.empty()) r.plugin_id = json_get_string(body, "plugin_id");
        auto cat = json_get_string(body, "category");
        r.category = cat;
        if (!qtext.empty() && r.path.empty()) {
          auto iq = interpreter_.interpret(qtext, cfg_, &history_);
          if (iq.results.empty()) {
            out.status = 404;
            out.body = json_ok(false, "\"error\":\"no results\"");
            return out;
          }
          r = iq.results.front();
        }
        bool ok = execute_result(r, cfg_, action);
        out.body = json_ok(ok);
        if (!ok) out.status = 400;
        return out;
      }
      if (path == "/backup" && req.method == "GET") {
        std::string err;
        auto dest = default_backup_path();
        if (!create_backup(cfg_, dest, cfg_.sync.include_index, err)) {
          out.status = 500;
          out.body = json_ok(false, "\"error\":\"" + json_escape(err) + "\"");
          return out;
        }
        std::string blob;
        read_file_all(dest, blob);
        out.content_type = "application/octet-stream";
        out.body = std::move(blob);
        return out;
      }
      if (path == "/backup" && req.method == "PUT") {
        auto tmp = default_backup_path() + ".http";
        if (!write_file_atomic(tmp, body.data(), body.size())) {
          out.status = 500;
          out.body = json_ok(false, "\"error\":\"write failed\"");
          return out;
        }
        std::string err;
        bool ok = restore_backup(tmp, err);
        out.body = json_ok(ok, ok ? "" : "\"error\":\"" + json_escape(err) + "\"");
        if (!ok) out.status = 400;
        return out;
      }
      if (path == "/sync/push" && req.method == "POST") {
        std::string err;
        bool ok = sync_push(cfg_, err);
        out.body = json_ok(ok, ok ? "" : "\"error\":\"" + json_escape(err) + "\"");
        if (!ok) out.status = 400;
        return out;
      }
      if (path == "/sync/pull" && req.method == "POST") {
        std::string err;
        bool ok = sync_pull(cfg_, err);
        out.body = json_ok(ok, ok ? "" : "\"error\":\"" + json_escape(err) + "\"");
        if (!ok) out.status = 400;
        return out;
      }
      if (path == "/snippets" && req.method == "POST") {
        Snippet s;
        s.trigger = json_get_string(body, "trigger");
        if (s.trigger.empty()) s.trigger = json_get_string(body, "name");
        s.id = json_get_string(body, "id");
        if (s.id.empty()) s.id = s.trigger;
        s.title = json_get_string(body, "title");
        s.body = json_get_string(body, "body");
        if (s.body.empty()) s.body = json_get_string(body, "text");
        s.kind = json_get_string(body, "kind");
        if (s.trigger.empty()) {
          out.status = 400;
          out.body = json_ok(false, "\"error\":\"trigger required\"");
          return out;
        }
        snippets_.upsert(std::move(s));
        snippets_.save();
        out.body = json_ok(true);
        return out;
      }
      if (path == "/snippets" && req.method == "GET") {
        std::ostringstream os;
        os << "{\"ok\":true,\"items\":[";
        bool first = true;
        for (auto& s : snippets_.all()) {
          if (!first) os << ',';
          first = false;
          os << "{\"id\":\"" << json_escape(s.id) << "\",\"trigger\":\"" << json_escape(s.trigger)
             << "\",\"title\":\"" << json_escape(s.title) << "\"}";
        }
        os << "]}";
        out.body = os.str();
        return out;
      }
      out.status = 404;
      out.body = json_ok(false, "\"error\":\"not found\"");
      return out;
    })) {
      log_warn("api", "failed to start HTTP API on " + cfg_.api.bind + ":" +
                          std::to_string(cfg_.api.port));
    }
  }

  std::thread indexer([this] {
    try {
      if (index_.store().live_count() == 0) index_.scan_roots();
    } catch (...) {
      log_warn("service", "initial scan failed");
    }
    auto last_vol_check = std::chrono::steady_clock::now();
    auto last_sync = std::chrono::steady_clock::now();
    auto volumes = list_volumes();
    while (running_) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      auto now = std::chrono::steady_clock::now();
      if (cfg_.sync.enabled && cfg_.sync.interval_seconds > 0 &&
          std::chrono::duration_cast<std::chrono::seconds>(now - last_sync).count() >=
              cfg_.sync.interval_seconds) {
        last_sync = now;
        std::string err;
        if (!sync_push(cfg_, err)) log_warn("sync", err);
      }
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
