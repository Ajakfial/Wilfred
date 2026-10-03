#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/expander.hpp"
#include "wilfred/search/snippets.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace wilfred {

class FsWatcher;
class GlobalHotkey;
class HttpApiServer;
class IpcServer;

class OverlayUi {
public:
  virtual ~OverlayUi() = default;
  virtual bool create() = 0;
  virtual void show() = 0;
  virtual void hide() = 0;
  virtual void destroy() = 0;
  virtual bool visible() const = 0;
};

std::unique_ptr<OverlayUi> create_overlay();

class Service {
public:
  Service();
  ~Service();

  int run_daemon();
  int run_search(const std::string& query, int limit);
  int run_preview(const std::string& path);
  int run_index_now();
  int run_status();
  int launch_by_query(const std::string& query);
  int run_backup(const std::string& dest, bool include_index);
  int run_restore(const std::string& src);
  int run_sync(bool push);
  // Automation triggers: run a named workflow against a search target
  // (empty target lists the workflow), or run any result action id
  // directly (`copy_path+reveal`, `workflow:review`, `media:play`, ...).
  int run_workflow(const std::string& name, const std::string& target);
  int run_exec(const std::string& action, const std::string& target);
  int run_convert(const std::vector<std::string>& args);
  int run_bgremove(const std::vector<std::string>& args);
  int run_import_list();
  int run_import_detect();
  int run_import(const std::string& launcher, const std::string& from_path, bool dry_run,
                 bool overwrite, bool include_hotkey, bool include_searches,
                 bool include_snippets, bool include_aliases, bool include_quicklinks,
                 bool include_theme, bool include_browser);

  Config& config() { return cfg_; }
  IndexEngine& index() { return index_; }

private:
  bool boot();
  void on_hotkey();
  void run_hotkey_action(const std::string& run);
  void apply_logging();

  Config cfg_;
  IndexEngine index_;
  SearchEngine search_;
  SnippetStore snippets_;
  GlobalExpander expander_;
  PluginHost plugins_;
  QueryInterpreter interpreter_;
  HistoryStore history_;
  std::unique_ptr<FsWatcher> watcher_;
  std::unique_ptr<GlobalHotkey> hotkey_;
  std::vector<std::unique_ptr<GlobalHotkey>> extra_hotkeys_;
  std::unique_ptr<IpcServer> ipc_;
  std::unique_ptr<HttpApiServer> http_;
  std::unique_ptr<OverlayUi> ui_;
  std::atomic<bool> running_{false};
  std::string last_overlay_query_;
};

}  // namespace wilfred
