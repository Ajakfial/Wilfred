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
#include "wilfred/browser/library.hpp"
#include "wilfred/import/import.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clip_history.hpp"
#include "wilfred/locale/locale.hpp"
#include "wilfred/search/pins.hpp"
#include "wilfred/ui/web_ui.hpp"
#include "wilfred/search/convert.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/expander.hpp"
#include "wilfred/search/layouts.hpp"
#include "wilfred/search/macros.hpp"
#include "wilfred/search/pkg.hpp"
#include "wilfred/search/quicknotes.hpp"
#include "wilfred/search/remote.hpp"
#include "wilfred/search/semantic.hpp"
#include "wilfred/search/suggest.hpp"
#include "wilfred/search/workflows.hpp"
#include "wilfred/sources/sources.hpp"
#include "wilfred/sync/backup.hpp"
#include "wilfred/ui/overlay.hpp"
#include "wilfred/ui/web_ui.hpp"
#include "wilfred/updater/updater.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

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
  for (auto& h : extra_hotkeys_)
    if (h) h->stop();
  if (watcher_) watcher_->stop();
  if (http_) http_->stop();
  if (ipc_) ipc_->stop();
  if (ui_) ui_->destroy();
  set_plugin_host_for_actions(nullptr);
  ClipStore::instance().save_now();
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
  ClipStore::instance().configure(cfg_.clipboard.manager ? cfg_.clipboard.max_entries : 0,
                                  cfg_.clipboard.persist, default_clips_path());
  if (cfg_.clipboard.manager && cfg_.clipboard.persist) ClipStore::instance().load();
  PinStore::instance().configure(default_pins_path(), cfg_.pins);
  PinStore::instance().load();
  LocaleStore::instance().configure(cfg_.ui.language);
  LocaleStore::instance().load();
  set_overlay_appearance(cfg_);
  snippets_.load(default_snippets_path(), cfg_);
  QuickNoteStore::instance().configure(path_join(data_directory(), "notes"));
  QuickNoteStore::instance().load();
  LayoutStore::instance().configure(path_join(data_directory(), "layouts"));
  TodoStore::instance().configure(path_join(data_directory(), "todos.txt"));
  TodoStore::instance().load();
  for (auto& d : default_plugin_directories()) create_directories(d);
  plugins_.load(cfg_);
  set_plugin_host_for_actions(&plugins_);
  if (cfg_.providers.semantic || cfg_.embedding.enabled)
    interpreter_.providers().add(std::make_unique<SemanticProvider>(index_));
  if (cfg_.sources.calendar)
    interpreter_.providers().add(std::make_unique<CalendarProvider>());
  if (cfg_.sources.contacts)
    interpreter_.providers().add(std::make_unique<ContactsProvider>());
  if (cfg_.sources.notes)
    interpreter_.providers().add(std::make_unique<NotesProvider>());
  if (cfg_.browser.library)
    interpreter_.providers().add(std::make_unique<BrowserLibraryProvider>());
  if (cfg_.remotes.enabled && !cfg_.remotes.sources.empty())
    interpreter_.providers().add(std::make_unique<RemoteProvider>());
  if (cfg_.packages.enabled)
    interpreter_.providers().add(std::make_unique<PkgProvider>());
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

// System-wide automation: run one hotkey binding action from anywhere.
// `show` toggles the overlay; macro/system/media/workflow actions run
// headless against the clipboard (workflows use targetless steps here).
void Service::run_hotkey_action(const std::string& run) {
  auto l = to_lower_utf8(run);
  while (!l.empty() && (l.front() == ' ' || l.front() == '\t')) l.erase(l.begin());
  if (l == "show") {
    on_hotkey();
    return;
  }
  if (l.rfind("system:", 0) == 0) {
    if (!native_system_action(run.substr(7)))
      log_warn("hotkey", "system action failed: " + run);
    return;
  }
  if (l.rfind("media:", 0) == 0) {
    std::string err;
    if (!native_media_action(run.substr(6), err))
      log_warn("hotkey", err.empty() ? "media action failed" : err);
    return;
  }
  if (l.rfind("macro:", 0) == 0) {
    auto text = run.substr(6);
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.erase(text.begin());
    ClipboardSnapshot clip;
    if (cfg_.search.clipboard) clip = read_clipboard();
    auto m = match_macro(text, cfg_);
    std::vector<SearchResult> cards;
    if (m.matched)
      cards = macro_results(m, clip.text);
    else {
      auto q = match_quicklink(text, cfg_);
      if (q.matched) cards = quicklink_results(q, clip.text);
    }
    if (cards.empty()) {
      log_warn("hotkey", "no macro matched: " + text);
      return;
    }
    execute_result(cards.front(), cfg_, "open");
    return;
  }
  if (l.rfind("workflow:", 0) == 0) {
    auto name = to_lower_utf8(run.substr(9));
    while (!name.empty() && (name.front() == ' ' || name.front() == '\t'))
      name.erase(name.begin());
    ClipboardSnapshot clip;
    if (cfg_.search.clipboard) clip = read_clipboard();
    SearchResult r;
    r.title = "Hotkey workflow";
    r.payload = clip.text;
    r.path = "";
    r.action = ResultAction::Copy;
    if (!execute_result(r, cfg_, "workflow:" + name))
      log_warn("hotkey", "workflow failed: " + name);
    return;
  }
  log_warn("hotkey", "unknown hotkey action: " + run);
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

int Service::run_workflow(const std::string& name, const std::string& target) {
  if (!boot()) return 1;
  auto key = to_lower_utf8(name);
  auto it = cfg_.workflows.find(key);
  if (it == cfg_.workflows.end()) {
    std::cerr << "unknown workflow \"" << name << "\"";
    if (!cfg_.workflows.empty()) {
      std::vector<std::string> names;
      for (auto& [k, _] : cfg_.workflows) names.push_back(k);
      std::sort(names.begin(), names.end());
      std::cerr << " (try:";
      for (auto& n : names) std::cerr << " " << n;
      std::cerr << ")";
    }
    std::cerr << "\n";
    return 1;
  }
  if (target.empty()) {
    std::cout << key << ": " << workflow_chain(key, cfg_) << "\n";
    return 0;
  }
  if (index_.store().live_count() == 0) index_.scan_roots();
  history_.record_query(target);
  auto iq = interpreter_.interpret(target, cfg_, &history_);
  const SearchResult* pick = nullptr;
  for (auto& r : iq.results) {
    if (r.action == ResultAction::Habit) continue;
    if ((r.path.empty() ? r.payload : r.path).empty()) continue;
    pick = &r;
    break;
  }
  if (!pick) {
    std::cerr << "no results\n";
    return 1;
  }
  auto file = pick->path.empty() ? pick->payload : pick->path;
  history_.record_selection(file);
  history_.record_choice(target, file);
  bool ok = execute_result(*pick, cfg_, "workflow:" + key);
  if (cfg_.history.persist) history_.save(default_history_path());
  std::cout << (ok ? "ok\n" : "workflow failed\n");
  return ok ? 0 : 1;
}

int Service::run_exec(const std::string& action, const std::string& target) {
  if (!boot()) return 1;
  if (action.empty()) {
    std::cerr << "usage: wilfred exec <action> [target]\n";
    return 2;
  }
  SearchResult r;
  r.title = target;
  r.path = target;
  r.payload = target;
  bool ok = execute_result(r, cfg_, action);
  std::cout << (ok ? "ok\n" : "action failed\n");
  return ok ? 0 : 1;
}

int Service::run_convert(const std::vector<std::string>& args) {
  if (!boot()) return 1;
  std::string src, fmt, dst;
  int rate = 0, channels = 0, bits = 0;
  std::vector<std::string> positional;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto need = [&](std::string& out) -> bool {
      if (i + 1 >= args.size()) {
        std::cerr << "usage: wilfred convert <src> [--to <fmt>] [--out <dst>] [--rate N] [--mono|--stereo] [--bits N]\n";
        return false;
      }
      out = args[++i];
      return true;
    };
    if (a == "--to" || a == "--format" || a == "--fmt" || a == "-t") {
      if (!need(fmt)) return 2;
    } else if (a == "--out" || a == "--output" || a == "-o") {
      if (!need(dst)) return 2;
    } else if (a == "--rate" || a == "--ar") {
      std::string v;
      if (!need(v)) return 2;
      try {
        rate = std::stoi(v);
      } catch (...) {
        std::cerr << "bad --rate " << v << "\n";
        return 2;
      }
    } else if (a == "--channels" || a == "--ac") {
      std::string v;
      if (!need(v)) return 2;
      try {
        channels = std::stoi(v);
      } catch (...) {
        std::cerr << "bad --channels " << v << "\n";
        return 2;
      }
    } else if (a == "--bits") {
      std::string v;
      if (!need(v)) return 2;
      try {
        bits = std::stoi(v);
      } catch (...) {
        std::cerr << "bad --bits " << v << "\n";
        return 2;
      }
    } else if (a == "--mono") {
      channels = 1;
    } else if (a == "--stereo") {
      channels = 2;
    } else if (a == "--help" || a == "-h" || a == "help") {
      std::cout << "usage: wilfred convert <src> [--to <fmt>] [--out <dst>] [--rate N] [--mono|--stereo] [--bits N]\n"
                   "  wilfred convert song.wav --to mp3\n"
                   "  wilfred convert song.wav out.ogg\n"
                   "  wilfred convert photo.bmp --to png\n";
      return 0;
    } else if (a == "to" || a == "as") {
      continue;
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "unknown option " << a << "\n";
      return 2;
    } else {
      positional.push_back(a);
    }
  }
  if (!positional.empty()) src = positional[0];
  if (positional.size() >= 2) {
    // Second positional is the target format or output path.
    if (dst.empty() && fmt.empty()) {
      const std::string& t = positional[1];
      bool looks_path = t.find('/') != std::string::npos || t.find('\\') != std::string::npos ||
                        t.find('.') != std::string::npos;
      if (looks_path && t.find('.') != std::string::npos) dst = t;
      else fmt = t;
    }
  }
  if (positional.size() > 2) {
    std::cerr << "usage: wilfred convert <src> [--to <fmt>] [--out <dst>]\n";
    return 2;
  }
  if (src.empty()) {
    std::cerr << "usage: wilfred convert <src> [--to <fmt>] [--out <dst>] [--rate N] [--mono|--stereo] [--bits N]\n";
    return 2;
  }
  if (dst.empty()) {
    if (fmt.empty()) {
      std::cerr << "usage: wilfred convert <src> [--to <fmt>] [--out <dst>]\n";
      return 2;
    }
    std::string err;
    dst = resolve_convert_output(src, fmt, err);
    if (dst.empty()) {
      std::cerr << err << "\n";
      return 1;
    }
  }
  std::string out_path, err;
  if (!convert_media_file(src, dst, rate, channels, bits, out_path, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  std::cout << out_path << "\n";
  return 0;
}

int Service::run_bgremove(const std::vector<std::string>& args) {
  if (!boot()) return 1;
  std::string src, dst, color;
  int tolerance = 32, feather = 2;
  bool contiguous = true;
  bool tol_set = false;
  std::vector<std::string> positional;
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto need = [&](std::string& out) -> bool {
      if (i + 1 >= args.size()) {
        std::cerr << "usage: wilfred bgremove <src> [--out <dst>] [--tolerance N] [--color #rrggbb] [--global|--contiguous] [--feather N]\n";
        return false;
      }
      out = args[++i];
      return true;
    };
    if (a == "--out" || a == "--output" || a == "-o") {
      if (!need(dst)) return 2;
    } else if (a == "--tolerance" || a == "--tol" || a == "-t") {
      std::string v;
      if (!need(v)) return 2;
      try {
        tolerance = std::stoi(v);
        tol_set = true;
      } catch (...) {
        std::cerr << "bad --tolerance " << v << "\n";
        return 2;
      }
    } else if (a == "--color" || a == "--bg" || a == "-c") {
      if (!need(color)) return 2;
    } else if (a == "--global" || a == "--chroma") {
      contiguous = false;
    } else if (a == "--contiguous" || a == "--flood") {
      contiguous = true;
    } else if (a == "--feather") {
      std::string v;
      if (!need(v)) return 2;
      try {
        feather = std::stoi(v);
      } catch (...) {
        std::cerr << "bad --feather " << v << "\n";
        return 2;
      }
    } else if (a == "--help" || a == "-h" || a == "help") {
      std::cout << "usage: wilfred bgremove <src> [--out <dst>] [--tolerance 0-100] [--color #rrggbb] [--global|--contiguous] [--feather 0-8]\n";
      return 0;
    } else if (!a.empty() && a[0] == '-') {
      std::cerr << "unknown option " << a << "\n";
      return 2;
    } else {
      positional.push_back(a);
    }
  }
  if (!positional.empty()) src = positional[0];
  // Convenience: `bgremove photo.png 40` and `bgremove photo.png out.png`.
  for (std::size_t i = 1; i < positional.size(); ++i) {
    const std::string& t = positional[i];
    std::uint8_t r = 0, g = 0, b = 0;
    bool is_color = parse_hex_color(t, r, g, b);
    std::string l = t;
    for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    bool is_named = l == "white" || l == "black" || l == "red" || l == "green" || l == "blue";
    int v = -1;
    try {
      std::size_t p = 0;
      int x = std::stoi(t, &p);
      if (p == t.size()) v = x;
    } catch (...) {
    }
    if (dst.empty() && (t.find('.') != std::string::npos)) {
      dst = t;
    } else if (color.empty() && (is_color || is_named)) {
      color = t;
    } else if (!tol_set && v >= 0 && v <= 100) {
      tolerance = v;
      tol_set = true;
    } else if (dst.empty()) {
      dst = t;
    } else {
      std::cerr << "unexpected argument " << t << "\n";
      return 2;
    }
  }
  if (src.empty()) {
    std::cerr << "usage: wilfred bgremove <src> [--out <dst>] [--tolerance 0-100] [--color #rrggbb]\n";
    return 2;
  }
  BgRemoveOptions opts;
  opts.tolerance = std::max(0, std::min(100, tolerance));
  opts.contiguous = contiguous;
  opts.feather = std::max(0, std::min(8, feather));
  if (!color.empty()) {
    std::uint8_t r = 0, g = 0, b = 0;
    std::string l = color;
    for (char& c : l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (l == "white") { r = 255; g = 255; b = 255; }
    else if (l == "black") { r = 0; g = 0; b = 0; }
    else if (l == "red") { r = 255; g = 0; b = 0; }
    else if (l == "green") { r = 0; g = 128; b = 0; }
    else if (l == "blue") { r = 0; g = 0; b = 255; }
    else if (!parse_hex_color(color, r, g, b)) {
      std::cerr << "bad --color " << color << " (try #rrggbb)\n";
      return 2;
    }
    opts.r = r;
    opts.g = g;
    opts.b = b;
    opts.has_color = true;
  }
  std::string out_path, err;
  if (!bgremove_file(src, dst, opts, out_path, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  std::cout << out_path << "\n";
  return 0;
}

int Service::run_index_now() {
  if (!boot()) return 1;
  index_.scan_roots();
  auto st = index_.stats();
  std::cout << "indexed " << st.files << " entries in " << st.last_scan_seconds << "s ("
            << st.errors << " errors)\n";
  return 0;
}

int Service::run_preview(const std::string& path) {
  auto pv = build_file_preview(path);
  if (!pv.error.empty()) {
    std::cerr << pv.error << ": " << path << "\n";
    return 1;
  }
  std::cout << pv.title << "\n" << pv.kind;
  if (!pv.size_label.empty()) std::cout << " · " << pv.size_label;
  if (!pv.modified_label.empty()) std::cout << " · " << pv.modified_label;
  std::cout << "\n";
  if (!pv.image_data_url.empty())
    std::cout << "[image " << pv.image_data_url.size() << " bytes]\n";
  if (!pv.text.empty()) std::cout << pv.text << "\n";
  return 0;
}

int Service::run_status() {  if (!boot()) return 1;
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
  std::cout << "plugins: " << plugins_.manifests().size()
            << " (registry=" << (cfg_.plugins.registry.empty() ? "off" : "on")
            << " approval=" << (cfg_.plugins.require_approval ? "on" : "off") << ")\n"
            << "snippets: " << snippets_.all().size() << "\n"
            << "layouts: auto_apply=" << (cfg_.layouts.auto_apply ? "on" : "off")
            << " auto_layout=" << (cfg_.layouts.auto_layout.empty() ? "-" : cfg_.layouts.auto_layout)
            << "\n"
            << "vectors: " << (index_.vectors().enabled() ? "on" : "off") << " ("
            << index_.vectors().size() << " backend=" << index_.vectors().backend() << ")\n"
            << "semantic: " << (cfg_.providers.semantic ? "on" : "off") << " ("
            << cfg_.providers.semantic_backend << ")\n"
            << "embedding: " << (cfg_.embedding.enabled ? "on" : "off") << "\n"
            << "ai: " << (cfg_.ai.enabled ? "on" : "off") << "\n"
            << "sources: calendar=" << (cfg_.sources.calendar ? "on" : "off")
            << " contacts=" << (cfg_.sources.contacts ? "on" : "off")
            << " notes=" << (cfg_.sources.notes ? "on" : "off")
            << " ocr=" << (cfg_.sources.ocr ? "on" : "off") << "\n"
            << "remotes: " << (cfg_.remotes.enabled ? "on" : "off") << " ("
            << cfg_.remotes.sources.size() << " sources)\n"
            << "packages: " << (cfg_.packages.enabled ? "on" : "off") << " (";
  {
    auto mgrs = pkg_detected_managers(cfg_);
    for (std::size_t i = 0; i < mgrs.size(); ++i) {
      if (i) std::cout << ",";
      std::cout << mgrs[i];
    }
    if (mgrs.empty()) std::cout << "none on PATH";
  }
  std::cout << ")\n"
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
  // Encrypted archives restore when sync.password/key_file is set; plain
  // archives always restore.
  std::string pw;
  std::string perr;
  if (resolve_sync_password(cfg_, pw, perr)) {
    if (!restore_backup_with_password(path, pw, e)) {
      std::cerr << e << "\n";
      return 1;
    }
  } else if (!restore_backup(path, e)) {
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

int Service::run_import_list() {
  auto all = import::supported_launchers();
  std::cout << "Supported launchers for `wilfred import`:\n";
  for (auto& li : all) {
    std::cout << "  " << li.id << "  (" << li.os << ") - " << li.name << "\n    " << li.description
              << "\n    format: " << li.format << "\n";
  }
  std::cout << "\nUsage:\n"
            << "  wilfred import --list                    Show this list\n"
            << "  wilfred import --detect                  Scan default locations\n"
            << "  wilfred import <id> [--from <path>] [--dry-run] [--overwrite]\n"
            << "  wilfred import auto [--dry-run]          Import from all detected launchers\n"
            << "  wilfred import --from <path>             Auto-detect format from file/dir\n";
  return 0;
}

int Service::run_import_detect() {
  auto found = import::detect_launchers();
  if (found.empty()) {
    std::cout << "No supported launchers detected in default locations.\n";
    std::cout << "Use `wilfred import --list` for supported apps, or "
              << "`wilfred import --from <path>` with an explicit export.\n";
    return 0;
  }
  std::cout << "Detected launchers:\n";
  for (auto& d : found) {
    std::cout << "  " << d.info.id << " (" << d.info.name << ", " << d.info.os << ")\n"
              << "    " << d.found_path << (d.is_directory ? "  [directory]" : "") << "\n";
  }
  return 0;
}

int Service::run_import(const std::string& launcher, const std::string& from_path, bool dry_run,
                        bool overwrite, bool include_hotkey, bool include_searches,
                        bool include_snippets, bool include_aliases, bool include_quicklinks,
                        bool include_theme, bool include_browser) {
  using namespace import;
  std::string lid = to_lower_utf8(launcher);
  while (!lid.empty() && (lid.front() == ' ' || lid.front() == '\t')) lid.erase(lid.begin());
  while (!lid.empty() && (lid.back() == ' ' || lid.back() == '\t')) lid.pop_back();
  if (lid.empty()) lid = from_path.empty() ? "auto" : "auto";

  ImportOptions opts;
  opts.overwrite = overwrite;
  opts.dry_run = dry_run;
  opts.include_hotkey = include_hotkey;
  opts.include_searches = include_searches;
  opts.include_snippets = include_snippets;
  opts.include_aliases = include_aliases;
  opts.include_quicklinks = include_quicklinks;
  opts.include_theme = include_theme;
  opts.include_browser = include_browser;

  std::vector<ImportedSettings> batches;
  std::vector<std::string> batch_labels;
  std::string fatal;

  auto is_dir_path = [](const std::string& p) {
    std::error_code ec;
    return std::filesystem::is_directory(std::filesystem::path(p), ec) && !ec;
  };

  auto parse_one_file = [&](const std::string& id, const std::string& path,
                            ImportedSettings& out) -> bool {
    if (is_dir_path(path)) {
      std::vector<std::string> w;
      std::string use_id = id;
      if (use_id == "auto") {
        use_id = detect_id_for_path(path, "");
        if (use_id == "auto") use_id = "alfred";
      }
      out = parse_directory(use_id, path, w);
      out.warnings.insert(out.warnings.end(), w.begin(), w.end());
      return true;
    }
    std::string text;
    if (!read_file_all(path, text)) {
      fatal = "unable to read " + path;
      return false;
    }
    std::vector<std::string> w;
    if (id == "auto") {
      std::string detected;
      std::string err;
      ImportedSettings tmp;
      if (!parse_file_auto(path, tmp, detected, err)) {
        fatal = err;
        return false;
      }
      out = std::move(tmp);
    } else {
      out = parse_text(id, text, path, w);
      out.warnings.insert(out.warnings.end(), w.begin(), w.end());
      if (out.searches.empty() && out.snippets.empty() && out.quicklinks.empty() &&
          out.aliases.empty() && !out.hotkey.has && !out.theme && !out.default_search_template) {
        fatal = "no importable settings found in " + path + " (as " + id + ")";
        if (!out.warnings.empty()) fatal += ": " + out.warnings.front();
        return false;
      }
    }
    return true;
  };

  if (!from_path.empty()) {
    std::string use_id = lid;
    if (use_id == "auto" || use_id == "all") {
      ImportedSettings one;
      std::string detected;
      std::string err;
      if (is_dir_path(from_path)) {
        std::vector<std::string> w;
        std::string guess = detect_id_for_path(from_path, "");
        if (guess == "auto") guess = "alfred";
        one = parse_directory(guess, from_path, w);
        one.warnings.insert(one.warnings.end(), w.begin(), w.end());
        detected = guess;
        if (one.searches.empty() && one.snippets.empty() && one.quicklinks.empty() &&
            !one.hotkey.has && !one.theme) {
          std::cerr << "no importable settings found under " << from_path << "\n";
          for (auto& wmsg : one.warnings) std::cerr << "  warn: " << wmsg << "\n";
          return 1;
        }
      } else if (!parse_file_auto(from_path, one, detected, err)) {
        std::cerr << err << "\n";
        return 1;
      }
      batches.push_back(std::move(one));
      batch_labels.push_back(from_path + " (detected as " + detected + ")");
    } else {
      const LauncherInfo* li = find_launcher(use_id);
      if (!li) {
        std::cerr << "unknown launcher \"" << launcher
                  << "\". Use `wilfred import --list`.\n";
        return 1;
      }
      if (!file_exists(from_path) && !is_dir_path(from_path)) {
        std::cerr << "file not found: " << from_path << "\n";
        return 1;
      }
      ImportedSettings one;
      if (is_dir_path(from_path)) {
        std::vector<std::string> w;
        one = parse_directory(li->id, from_path, w);
        one.warnings.insert(one.warnings.end(), w.begin(), w.end());
      } else {
        std::string text;
        if (!read_file_all(from_path, text)) {
          std::cerr << "unable to read " << from_path << "\n";
          return 1;
        }
        std::vector<std::string> w;
        one = parse_text(li->id, text, from_path, w);
        one.warnings.insert(one.warnings.end(), w.begin(), w.end());
        if (one.searches.empty() && one.snippets.empty() && one.quicklinks.empty() &&
            one.aliases.empty() && !one.hotkey.has && !one.theme &&
            !one.default_search_template) {
          std::cerr << "no importable settings found in " << from_path << " (as " << li->id
                    << ")\n";
          for (auto& wmsg : one.warnings) std::cerr << "  warn: " << wmsg << "\n";
          return 1;
        }
      }
      batches.push_back(std::move(one));
      batch_labels.push_back(from_path + " (as " + li->id + ")");
    }
  } else {
    // No --from: use default locations.
    if (lid == "auto" || lid == "all") {
      auto found = detect_launchers();
      if (found.empty()) {
        std::cerr << "No supported launchers detected. Use `wilfred import --list` and "
                  << "`wilfred import <id> --from <path>`.\n";
        return 1;
      }
      for (auto& d : found) {
        ImportedSettings one;
        if (!parse_one_file(d.info.id, d.found_path, one)) {
          std::cerr << "warn: skipping " << d.found_path << ": " << fatal << "\n";
          fatal.clear();
          continue;
        }
        batches.push_back(std::move(one));
        batch_labels.push_back(d.found_path + " (" + d.info.id + ")");
      }
      if (batches.empty()) {
        std::cerr << "Nothing importable found.\n";
        return 1;
      }
    } else {
      const LauncherInfo* li = find_launcher(lid);
      if (!li) {
        std::cerr << "unknown launcher \"" << launcher << "\". Use `wilfred import --list`.\n";
        return 1;
      }
      bool any = false;
      for (auto& cand : li->candidates) {
        if (!file_exists(cand)) continue;
        ImportedSettings one;
        if (!parse_one_file(li->id, cand, one)) continue;
        batches.push_back(std::move(one));
        batch_labels.push_back(cand + " (" + li->id + ")");
        any = true;
      }
      // Wox shares Flow's layout; if user asked for wox but only flow paths exist
      // (or vice versa), try the sibling id as a fallback.
      if (!any && (li->id == "wox" || li->id == "flowlauncher")) {
        const LauncherInfo* other =
            find_launcher(li->id == "wox" ? "flowlauncher" : "wox");
        if (other) {
          for (auto& cand : other->candidates) {
            if (!file_exists(cand)) continue;
            ImportedSettings one;
            if (!parse_one_file(other->id, cand, one)) continue;
            batches.push_back(std::move(one));
            batch_labels.push_back(cand + " (" + other->id + ")");
            any = true;
          }
        }
      }
      if (!any) {
        std::cerr << "No " << li->name << " config found in default locations.\n";
        std::cerr << "Looked in:\n";
        for (auto& cand : li->candidates) std::cerr << "  " << cand << "\n";
        std::cerr << "Pass an explicit file/dir with --from <path>.\n";
        return 1;
      }
    }
  }

  // Load (or create) current Wilfred config without starting the index.
  ConfigError cerr;
  Config cfg = load_or_create_user_config(cerr);
  if (!cerr.message.empty()) log_warn("config", cerr.message);

  ImportCounts total;
  std::vector<std::string> all_warnings;
  int batch_no = 0;
  for (auto& b : batches) {
    std::vector<std::string> w;
    ImportCounts c = apply_settings(cfg, b, opts, w);
    total.macros_added += c.macros_added;
    total.macros_overwritten += c.macros_overwritten;
    total.macros_skipped += c.macros_skipped;
    total.quicklinks_added += c.quicklinks_added;
    total.quicklinks_overwritten += c.quicklinks_overwritten;
    total.quicklinks_skipped += c.quicklinks_skipped;
    total.snippets_added += c.snippets_added;
    total.snippets_overwritten += c.snippets_overwritten;
    total.snippets_skipped += c.snippets_skipped;
    total.aliases_added += c.aliases_added;
    total.aliases_overwritten += c.aliases_overwritten;
    total.aliases_skipped += c.aliases_skipped;
    if (c.hotkey_applied) total.hotkey_applied = true;
    if (c.theme_applied) total.theme_applied = true;
    if (c.browser_applied) total.browser_applied = true;
    for (auto& msg : w)
      all_warnings.push_back("[" + batch_labels[static_cast<std::size_t>(batch_no)] + "] " + msg);
    ++batch_no;
  }

  auto print_summary = [&] {
    std::cout << (dry_run ? "Import preview (dry run, nothing written):\n"
                          : "Import result:\n");
    for (auto& l : batch_labels) std::cout << "  source: " << l << "\n";
    std::cout << "  macros: +" << total.macros_added << " new, " << total.macros_overwritten
              << " overwritten, " << total.macros_skipped << " skipped\n";
    std::cout << "  quicklinks: +" << total.quicklinks_added << " new, "
              << total.quicklinks_overwritten << " overwritten, " << total.quicklinks_skipped
              << " skipped\n";
    std::cout << "  snippets: +" << total.snippets_added << " new, "
              << total.snippets_overwritten << " overwritten, " << total.snippets_skipped
              << " skipped\n";
    std::cout << "  aliases: +" << total.aliases_added << " new, " << total.aliases_overwritten
              << " overwritten, " << total.aliases_skipped << " skipped\n";
    std::cout << "  hotkey: " << (total.hotkey_applied ? "imported" : "unchanged") << "\n";
    std::cout << "  theme: " << (total.theme_applied ? "imported" : "unchanged") << "\n";
    std::cout << "  default search: " << (total.browser_applied ? "imported" : "unchanged")
              << "\n";
    if (!all_warnings.empty()) {
      std::cout << "  warnings:\n";
      for (auto& wmsg : all_warnings) std::cout << "    - " << wmsg << "\n";
    }
  };

  if (dry_run) {
    print_summary();
    return 0;
  }

  // Safety backup of the pre-import config.
  {
    std::string orig;
    if (read_file_all(cfg.source_path.empty() ? default_config_path() : cfg.source_path, orig)) {
      std::string bak = (cfg.source_path.empty() ? default_config_path() : cfg.source_path) +
                        ".pre-import.bak";
      create_directories(path_parent(bak));
      write_file_atomic(bak, orig.data(), orig.size());
      std::cout << "Backed up existing config to " << bak << "\n";
    }
  }
  std::string dest = cfg.source_path.empty() ? default_config_path() : cfg.source_path;
  std::string err;
  if (!save_config_file(dest, cfg, err)) {
    std::cerr << err << "\n";
    return 1;
  }
  cfg_ = cfg;
  print_summary();
  std::cout << "Wrote " << dest << "\n";
  return 0;
}

int Service::run_daemon() {
  if (!boot()) return 1;
  running_ = true;
  g_running = &running_;

  // Background update check on startup
  startup_update_check();
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

  // Global snippet expansion in any app (opt-in via snippets.global_expansion).
  if (cfg_.snippets.global_expansion) expander_.start(cfg_, &snippets_);

  overlay_bind(
      [this](const std::string& q) {
        last_overlay_query_ = q;
        auto iq = interpreter_.interpret(q, cfg_, &history_);
        OverlayResponse resp;
        resp.results = std::move(iq.results);
        resp.query = q;
        try {
          auto assist = build_assist(q, cfg_, &history_, &index_);
          resp.correction = std::move(assist.correction);
          resp.ghost = std::move(assist.ghost);
          resp.candidates = std::move(assist.candidates);
        } catch (...) {
        }
        return resp;
      },
      [this](const SearchResult& r, const std::string& action_id) {
        if (r.action == ResultAction::Habit) return;
        auto hide = action_hides_overlay(action_id);
        if (r.action == ResultAction::Screenshot || r.category == "screenshot") {
          if (hide && ui_) ui_->hide();
          // Let the overlay hide (plus the settle delay in execute_result_action)
          // so the overlay itself is not in the capture.
          std::this_thread::sleep_for(std::chrono::milliseconds(350));
          execute_result(r, cfg_, action_id);
          return;
        }
        if (r.action == ResultAction::SwitchWindow || r.category == "window") {
          if (hide && ui_) ui_->hide();
          execute_result(r, cfg_, action_id);
          return;
        }
        if (r.action == ResultAction::System || r.category == "system") {
          if (hide && ui_) ui_->hide();
          execute_result(r, cfg_, action_id);
          return;
        }
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
  // Extra system-wide bindings from `hotkeys:` (each gets its own backend
  // instance; a failed binding only warns).
  for (auto& b : cfg_.hotkeys) {
    auto hk = std::make_unique<GlobalHotkey>();
    std::string run = b.run;
    std::string label = b.name.empty() ? run : b.name;
    if (hk->start_binding(b.modifiers, b.key, cfg_.hotkey.use_command_on_macos,
                          [this, run] { run_hotkey_action(run); })) {
      extra_hotkeys_.push_back(std::move(hk));
    } else {
      log_warn("hotkey", "failed to register extra hotkey: " + label);
    }
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
      } else if (req.cmd == "preview") {
        resp.text = overlay_preview_json(req.path.empty() ? req.query : req.path);
        resp.ok = true;
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
    auto last_mon_check = std::chrono::steady_clock::now() - std::chrono::seconds(30);
    std::string last_mon_sig;
    auto volumes = list_volumes();
    while (running_) {
      std::this_thread::sleep_for(std::chrono::seconds(2));
      auto now = std::chrono::steady_clock::now();
      if (cfg_.layouts.auto_apply &&
          std::chrono::duration_cast<std::chrono::seconds>(now - last_mon_check).count() >= 5) {
        last_mon_check = now;
        std::string sig, serr;
        if (native_monitor_signature(sig, serr) && !sig.empty() && sig != last_mon_sig) {
          bool first = last_mon_sig.empty();
          last_mon_sig = sig;
          if (!first) {
            std::string layout;
            for (auto& [key, name] : cfg_.layouts.monitor_layouts) {
              if (!key.empty() && sig.find(key) != std::string::npos) {
                layout = name;
                break;
              }
            }
            if (layout.empty()) layout = cfg_.layouts.auto_layout;
            if (!layout.empty()) {
              std::string err;
              if (LayoutStore::apply_layout_by_name(layout, err))
                log_info("layouts", "auto-applied '" + layout + "' for monitors " + sig);
              else
                log_warn("layouts", err.empty() ? "auto-apply failed" : err);
            }
          }
        }
      }
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
