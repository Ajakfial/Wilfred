#include "wilfred/platform/android_bridge.hpp"

#include "wilfred/browser/library.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/core/json.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clip_history.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/layouts.hpp"
#include "wilfred/search/quicknotes.hpp"
#include "wilfred/search/semantic.hpp"
#include "wilfred/search/snippets.hpp"
#include "wilfred/search/suggest.hpp"
#include "wilfred/sources/sources.hpp"

#include <filesystem>
#include <fstream>
#include <mutex>

namespace fs = std::filesystem;

namespace wilfred {

struct MobileCore::Impl {
  bool booted{false};
  std::string files_dir;
  std::string config_path;
  std::string index_dir;
  std::string history_path;
  std::string snippets_path;
  std::string clips_path;
  std::string log_path;

  Config cfg;
  IndexEngine index;
  SearchEngine search{index};
  SnippetStore snippets;
  PluginHost plugins;
  QueryInterpreter interpreter{index, search, &snippets, &plugins};
  HistoryStore history;
  // Last search results so Kotlin can request per-result actions and run
  // C++ side-effect actions (timer_stop, note_delete, todo_done, clip_*,
  // copy_*, ...) by index.
  std::vector<SearchResult> last_results;
  std::string last_query;
  std::mutex mu;
};

MobileCore::MobileCore() : impl_(new Impl()) {}
MobileCore::~MobileCore() { delete impl_; }

bool MobileCore::boot(const std::string& files_dir, std::string& error) {
  if (files_dir.empty()) {
    error = "files_dir is empty";
    return false;
  }
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  st.files_dir = files_dir;
  st.config_path = path_join(files_dir, "wilfred.yml");
  st.index_dir = path_join(files_dir, "index");
  st.history_path = path_join(files_dir, "history.bin");
  st.snippets_path = path_join(files_dir, "snippets.yml");
  st.clips_path = path_join(files_dir, "clips.bin");
  st.log_path = path_join(files_dir, "wilfred.log");

  create_directories(files_dir);
  create_directories(st.index_dir);

  Config cfg;
  if (file_exists(st.config_path)) {
    ConfigError cerr;
    if (!load_config_file(st.config_path, cfg, cerr)) {
      error = cerr.message.empty() ? ("cannot parse " + st.config_path) : cerr.message;
      return false;
    }
    cfg.source_path = st.config_path;
  } else {
    cfg.source_path = st.config_path;
    cfg.index.system_directories = default_system_directories();
  }
  st.cfg = cfg;

  Logger::instance().set_level(LogLevel::Info);
  Logger::instance().set_file(st.log_path, cfg.logging.max_file_bytes);

  if (!st.index.open(st.index_dir, st.cfg)) {
    error = "cannot open index at " + st.index_dir;
    return false;
  }
  st.history.set_enabled(st.cfg.history.enabled);
  st.history.set_max(st.cfg.history.max_entries);
  if (st.cfg.history.persist) st.history.load(st.history_path);
  ClipStore::instance().configure(st.cfg.clipboard.manager ? st.cfg.clipboard.max_entries : 0,
                                  st.cfg.clipboard.persist, st.clips_path);
  if (st.cfg.clipboard.manager && st.cfg.clipboard.persist) ClipStore::instance().load();
  st.snippets.load(st.snippets_path, st.cfg);
  QuickNoteStore::instance().configure(path_join(files_dir, "notes"));
  QuickNoteStore::instance().load();
  LayoutStore::instance().configure(path_join(files_dir, "layouts"));
  TodoStore::instance().configure(path_join(files_dir, "todos.txt"));
  TodoStore::instance().load();
  st.plugins.load(st.cfg);
  if (st.cfg.providers.semantic || st.cfg.embedding.enabled)
    st.interpreter.providers().add(std::make_unique<SemanticProvider>(st.index));
  if (st.cfg.sources.calendar) st.interpreter.providers().add(std::make_unique<CalendarProvider>());
  if (st.cfg.sources.contacts) st.interpreter.providers().add(std::make_unique<ContactsProvider>());
  if (st.cfg.sources.notes) st.interpreter.providers().add(std::make_unique<NotesProvider>());
  if (st.cfg.browser.library)
    st.interpreter.providers().add(std::make_unique<BrowserLibraryProvider>());
  st.booted = true;
  return true;
}

std::string MobileCore::action_to_string(int action_value) {
  auto a = static_cast<ResultAction>(action_value);
  switch (a) {
    case ResultAction::Open: return "open";
    case ResultAction::Reveal: return "reveal";
    case ResultAction::Copy: return "copy";
    case ResultAction::WebSearch: return "web";
    case ResultAction::Calculate: return "calc";
    case ResultAction::Convert: return "convert";
    case ResultAction::None: return "none";
    case ResultAction::Habit: return "habit";
    case ResultAction::Mini: return "mini";
    case ResultAction::Expand: return "expand";
    case ResultAction::Plugin: return "plugin";
    case ResultAction::SwitchWindow: return "window";
    case ResultAction::System: return "system";
    case ResultAction::Screenshot: return "screenshot";
  }
  return "open";
}

std::string MobileCore::results_to_json(const std::vector<SearchResult>& results,
                                         std::size_t limit) {
  std::string out = "[";
  bool first = true;
  std::size_t n = 0;
  for (auto& r : results) {
    if (n >= limit) break;
    ++n;
    if (!first) out.push_back(',');
    first = false;
    out += "{\"title\":\"" + json_escape(r.title) + "\",\"subtitle\":\"" +
           json_escape(r.subtitle) + "\",\"path\":\"" + json_escape(r.path) +
           "\",\"payload\":\"" + json_escape(r.payload) + "\",\"score\":" +
           std::to_string(r.score) + ",\"action\":\"" +
           action_to_string(static_cast<int>(r.action)) + "\",\"category\":\"" +
           json_escape(r.category) + "\",\"kind\":\"" + json_escape(r.kind_label) +
           "\",\"actions\":[";
    bool afirst = true;
    for (auto& a : r.actions) {
      if (!afirst) out.push_back(',');
      afirst = false;
      out += "{\"id\":\"" + json_escape(a.id) + "\",\"label\":\"" + json_escape(a.label) +
             "\"}";
    }
    out += "]}";
  }
  out.push_back(']');
  return out;
}

std::string MobileCore::search_json(const std::string& query, int limit) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted) return "[]";
  if (limit <= 0) limit = 40;
  if (limit > 100) limit = 100;
  try {
    st.history.record_query(query);
    auto iq = st.interpreter.interpret(query, st.cfg, &st.history);
    if (st.cfg.history.persist) st.history.save(st.history_path);
    st.last_results = iq.results;
    st.last_query = query;
    return results_to_json(iq.results, static_cast<std::size_t>(limit));
  } catch (...) {
    return "[]";
  }
}

std::string MobileCore::status_json() {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted) return "{\"ok\":false}";
  auto s = st.index.stats();
  return "{\"ok\":true,\"records\":" + std::to_string(s.files) + ",\"dirs\":" +
         std::to_string(s.dirs) + ",\"apps\":" + std::to_string(s.apps) + "}";
}

bool MobileCore::index_now(std::string& error) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted) {
    error = "not booted";
    return false;
  }
  try {
    st.index.scan_roots();
    return true;
  } catch (const std::exception& ex) {
    error = ex.what();
    return false;
  } catch (...) {
    error = "index failed";
    return false;
  }
}

bool MobileCore::register_app(const std::string& name, const std::string& package_id,
                               const std::string& label) {
  if (name.empty() || package_id.empty()) return false;
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted) return false;
  // Index as an application record; path carries the launch id the Kotlin
  // side turns into a launch Intent (package:<id>).
  try {
    IndexRecord rec;
    rec.kind = FileKind::Application;
    rec.flags = RecordFlags::Application | RecordFlags::Executable;
    std::string path = "package:" + package_id;
    std::uint32_t id = st.index.store().upsert(rec, path);
    // Human-readable names live in content tokens since the record name is
    // derived from the package: path.
    std::vector<std::string> toks = tokenize_name(name);
    auto more = tokenize_name(package_id);
    toks.insert(toks.end(), more.begin(), more.end());
    if (!label.empty() && label != name) {
      auto lt = tokenize_name(label);
      toks.insert(toks.end(), lt.begin(), lt.end());
    }
    if (!toks.empty()) st.index.store().add_content_tokens(id, toks);
    st.index.bump_generation();
    return true;
  } catch (...) {
    return false;
  }
}

bool MobileCore::record_choice(const std::string& query, const std::string& key) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted || key.empty()) return false;
  st.history.record_selection(key);
  st.history.record_choice(query, key);
  if (st.cfg.history.persist) st.history.save(st.history_path);
  return true;
}

void MobileCore::set_clipboard(const std::string& text) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  ClipboardSnapshot snap;
  snap.text = text;
  set_clipboard_override(snap);
}

std::string MobileCore::clipboard_text() {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted) return {};
  try {
    return read_clipboard().text;
  } catch (...) {
    return {};
  }
}

std::string MobileCore::assist_json(const std::string& query) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted) return "{\"correction\":\"\",\"ghost\":\"\",\"candidates\":[]}";
  try {
    AssistResult a = build_assist(query, st.cfg, &st.history, &st.index);
    std::string out = "{\"correction\":\"" + json_escape(a.correction) + "\",\"ghost\":\"" +
                      json_escape(a.ghost) + "\",\"candidates\":[";
    bool first = true;
    for (auto& c : a.candidates) {
      if (!first) out.push_back(',');
      first = false;
      out += "\"" + json_escape(c) + "\"";
    }
    out += "]}";
    return out;
  } catch (...) {
    return "{\"correction\":\"\",\"ghost\":\"\",\"candidates\":[]}";
  }
}

std::string MobileCore::actions_json(std::size_t result_index) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted || result_index >= st.last_results.size()) return "[]";
  try {
    const auto& r = st.last_results[result_index];
    std::string out = "[";
    bool first = true;
    for (auto& a : r.actions) {
      if (!first) out.push_back(',');
      first = false;
      out += "{\"id\":\"" + json_escape(a.id) + "\",\"label\":\"" + json_escape(a.label) +
             "\"}";
    }
    out.push_back(']');
    return out;
  } catch (...) {
    return "[]";
  }
}

bool MobileCore::execute_action(std::size_t result_index, const std::string& action_id,
                                 std::string& error) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted) {
    error = "not booted";
    return false;
  }
  if (result_index >= st.last_results.size()) {
    error = "bad result index";
    return false;
  }
  try {
    SearchResult r = st.last_results[result_index];
    if (!execute_result_action(r, st.cfg, action_id)) {
      error = "action failed";
      return false;
    }
    return true;
  } catch (const std::exception& ex) {
    error = ex.what();
    return false;
  } catch (...) {
    error = "action failed";
    return false;
  }
}

std::string MobileCore::preview_json(const std::string& path) {
  Impl& st = *impl_;
  std::lock_guard<std::mutex> lock(st.mu);
  if (!st.booted || path.empty() || path.rfind("package:", 0) == 0 ||
      path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0) {
    return "{\"exists\":false}";
  }
  try {
    std::error_code ec;
    fs::path p(path);
    bool exists = fs::exists(p, ec);
    if (ec || !exists) return "{\"exists\":false}";
    bool is_dir = fs::is_directory(p, ec);
    std::uintmax_t size = 0;
    if (!is_dir) {
      size = fs::file_size(p, ec);
      if (ec) size = 0;
    }
    std::string preview;
    if (is_dir) {
      int n = 0;
      for (auto it = fs::directory_iterator(p, ec);
           it != fs::directory_iterator() && n < 12; it.increment(ec)) {
        if (ec) break;
        if (n) preview += "\n";
        preview += it->path().filename().string();
        if (++n >= 12) break;
      }
    } else if (size < 512 * 1024) {
      std::ifstream f(p, std::ios::binary);
      if (f) {
        char buf[4096];
        f.read(buf, sizeof(buf));
        std::streamsize got = f.gcount();
        preview.assign(buf, buf + got);
        // Strip NULs for JSON safety; binary files show a placeholder.
        bool binary = false;
        for (char c : preview) {
          if (c == '\0') {
            binary = true;
            break;
          }
        }
        if (binary) preview = "<binary file>";
        if (preview.size() > 2000) preview.resize(2000);
      }
    } else {
      preview = "<large file>";
    }
    std::string name = path_filename(path);
    return "{\"exists\":true,\"is_dir\":" + std::string(is_dir ? "true" : "false") +
           ",\"size\":" + std::to_string(size) + ",\"name\":\"" + json_escape(name) +
           "\",\"preview\":\"" + json_escape(preview) + "\"}";
  } catch (...) {
    return "{\"exists\":false}";
  }
}

}  // namespace wilfred
