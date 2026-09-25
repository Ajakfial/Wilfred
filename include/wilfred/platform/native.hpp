#pragma once

#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/fs/volumes.hpp"
#include "wilfred/fs/watcher.hpp"
#include "wilfred/hotkey/hotkey.hpp"

#include <memory>
#include <string>
#include <vector>

namespace wilfred {

struct WatcherBackend {
  virtual ~WatcherBackend() = default;
  virtual bool start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb) = 0;
  virtual void stop() = 0;
  virtual void add_root(const std::string& root) = 0;
};

std::unique_ptr<WatcherBackend> create_watcher_backend();

struct HotkeyBackend {
  virtual ~HotkeyBackend() = default;
  virtual bool start(const Config& cfg, HotkeyFn cb) = 0;
  virtual void stop() = 0;
};

std::unique_ptr<HotkeyBackend> create_hotkey_backend();

std::vector<AppInfo> native_discover_apps();
std::string native_default_browser_id();
std::string native_default_browser_executable();
std::vector<BrowserInfo> native_list_browsers();
bool native_launch(const std::string& path);
bool native_reveal(const std::string& path);
bool native_open_url(const std::string& url);
std::vector<VolumeInfo> native_list_volumes();

}  // namespace wilfred
