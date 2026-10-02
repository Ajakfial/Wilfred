#pragma once

#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/fs/volumes.hpp"
#include "wilfred/fs/watcher.hpp"
#include "wilfred/hotkey/hotkey.hpp"

#include <cstddef>
#include <cstdint>
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

struct NativeWindowInfo {
  std::uint64_t id{0};
  std::string title;
  std::string owner;
};

std::vector<NativeWindowInfo> native_list_windows();
bool native_focus_window(std::uint64_t id);

// lock | sleep | shutdown | restart | logout | empty_trash
bool native_system_action(const std::string& id);

// Process control + media keys (search/media.hpp re-exports these so minis and
// actions can share one implementation).
bool native_kill_process(std::uint32_t pid, std::string& error);
// play | pause | playpause | next | prev | stop | mute | volup | voldn
bool native_media_action(const std::string& id, std::string& error);

enum class NativeScreenshotMode { Fullscreen, Window, Region };

// Captures a screenshot. On success returns true; out_path is the saved
// image file, or empty when the platform handed off to an interactive OS
// picker that manages its own output. On failure returns false with error set.
bool native_take_screenshot(NativeScreenshotMode mode, std::string& out_path,
                            std::string& error);
std::string native_screenshot_save_directory();

struct OpenWithApp {
  std::string name;    // display name, e.g. "Visual Studio Code"
  std::string target;  // opaque id passed back to native_open_with
};

// Up to max_apps applications that can open path, best first.
// Empty when none are known (callers fall back to the default handler).
std::vector<OpenWithApp> native_apps_for_file(const std::string& path, std::size_t max_apps);
bool native_open_with(const std::string& target, const std::string& file);
bool native_open_terminal(const std::string& dir);
bool native_open_editor(const std::string& dir);

}  // namespace wilfred
