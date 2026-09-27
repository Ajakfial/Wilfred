#pragma once

#include "wilfred/config/config.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/snippets.hpp"

#include <atomic>
#include <memory>
#include <string>

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
  int run_index_now();
  int run_status();
  int launch_by_query(const std::string& query);
  int run_backup(const std::string& dest, bool include_index);
  int run_restore(const std::string& src);
  int run_sync(bool push);

  Config& config() { return cfg_; }
  IndexEngine& index() { return index_; }

private:
  bool boot();
  void on_hotkey();
  void apply_logging();

  Config cfg_;
  IndexEngine index_;
  SearchEngine search_;
  SnippetStore snippets_;
  PluginHost plugins_;
  QueryInterpreter interpreter_;
  HistoryStore history_;
  std::unique_ptr<FsWatcher> watcher_;
  std::unique_ptr<GlobalHotkey> hotkey_;
  std::unique_ptr<IpcServer> ipc_;
  std::unique_ptr<HttpApiServer> http_;
  std::unique_ptr<OverlayUi> ui_;
  std::atomic<bool> running_{false};
  std::string last_overlay_query_;
};

}  // namespace wilfred
