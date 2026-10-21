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
  // One extra binding from cfg.hotkeys. Default: unsupported (the primary
  // summon hotkey keeps working through start()).
  virtual bool start_binding(const std::vector<std::string>& /*modifiers*/,
                             const std::string& /*key*/, bool /*use_command_on_macos*/,
                             HotkeyFn /*cb*/) {
    return false;
  }
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

// Window management ops shared by the `windows` mini, layouts, and
// window workflow steps. All three platforms implement these; Linux
// without X11 reports unsupported via error.
enum class NativeWindowOp { Minimize, Maximize, Restore, Close, SnapLeft, SnapRight };

struct NativeWindowRect {
  int x{0};
  int y{0};
  int w{0};
  int h{0};
  bool maximized{false};
};

bool native_window_action(std::uint64_t id, NativeWindowOp op, std::string& error);
bool native_window_rect(std::uint64_t id, NativeWindowRect& rect, std::string& error);
bool native_window_move(std::uint64_t id, int x, int y, int w, int h, std::string& error);

// lock | sleep | shutdown | restart | logout | empty_trash
bool native_system_action(const std::string& id);

// --- System toggles: wifi / bluetooth / volume / brightness ---
// All return true on success, false with error set when unsupported or the
// OS tool failed. Mobile stubs return false ("not supported on mobile").
bool native_wifi_status(bool& enabled, std::string& detail, std::string& error);
bool native_wifi_set(bool enabled, std::string& error);
std::vector<std::string> native_wifi_list(std::string& error);
bool native_bluetooth_status(bool& enabled, std::string& detail, std::string& error);
bool native_bluetooth_set(bool enabled, std::string& error);
// level 0..100. muted is the OS mute flag (independent of level).
bool native_volume_status(int& level, bool& muted, std::string& error);
bool native_volume_set(int level, std::string& error);
bool native_volume_mute(bool mute, std::string& error);
// percent 0..100.
bool native_brightness_status(int& percent, std::string& error);
bool native_brightness_set(int percent, std::string& error);
// Do Not Disturb / Focus mode. `enabled` is true when notifications are
// silenced. Best-effort per platform: Windows notification setting, GNOME
// show-banners, macOS legacy Notification Center flag; mobile is unsupported.
bool native_focus_status(bool& enabled, std::string& detail, std::string& error);
bool native_focus_set(bool enabled, std::string& error);

// --- Autostart / login items ---
// enable registers this binary to launch at login (bare `wilfred` runs the
// daemon); disable removes our entry. status reports whether OUR entry is
// present and pointing at this executable (detail says what it points at).
// All return true on success, false with error set when unsupported or the
// OS call failed. Mobile stubs return false ("not supported on mobile").
bool native_autostart_status(bool& enabled, std::string& detail, std::string& error);
bool native_autostart_set(bool enabled, std::string& error);
// Pure content builders for the file-based backends (macOS LaunchAgents
// plist, Linux/BSD XDG autostart entry). Tested without touching the machine.
std::string autostart_desktop_entry(const std::string& exe);
std::string autostart_launchd_plist(const std::string& exe);

// --- Settings deep-links ---
// page is lowercased, e.g. "", "wifi", "network", "bluetooth", "sound",
// "display", "battery", "power", "apps", "privacy", "update".
// Empty page opens the main settings app.
bool native_open_settings(const std::string& page, std::string& error);

// --- Displays: tiling + monitor-change detection ---
// Work area of the primary monitor (excludes taskbar/dock when known).
struct NativeWorkArea {
  int x{0};
  int y{0};
  int w{0};
  int h{0};
};
bool native_primary_work_area(NativeWorkArea& out, std::string& error);
// Stable signature of the current monitor set, e.g.
// "1920x1080@0,0|2560x1440@1920,0". Empty with error when unknown.
// The daemon polls this to auto-apply layouts; mobile returns unsupported.
bool native_monitor_signature(std::string& sig, std::string& error);

// Process control + media keys (search/media.hpp re-exports these so minis and
// actions can share one implementation).
bool native_kill_process(std::uint32_t pid, std::string& error);
// play | pause | playpause | next | prev | stop | mute | volup | voldn
bool native_media_action(const std::string& id, std::string& error);

enum class NativeScreenshotMode { Fullscreen, Window, Region };

// Captures a screenshot. On success returns true; out_path is the saved
// image file, or empty when the platform handed off to an interactive OS
// picker that manages its own output. On failure returns false with error set.
bool native_take_screenshot(NativeScreenshotMode mode, std::string& out_path, std::string& error);
std::string native_screenshot_save_directory();

// --- Screen color sampling ---
// 8-bit sRGB channels, as displayed. Not linearized, not premultiplied.
struct NativeColor {
  unsigned char r{0};
  unsigned char g{0};
  unsigned char b{0};
};

// Samples the pixel under the mouse cursor in virtual-screen coordinates.
// The caller is responsible for the magnifier/overlay; this only reads a point.
// Returns false with error set where the platform cannot read the screen:
// iOS and Android (sandboxed), and Wayland compositors without a screencopy
// portal. Those are expected outcomes, not bugs.
bool native_color_at_cursor(NativeColor& out, std::string& error);

// Same, for an explicit point in virtual-screen coordinates.
bool native_color_at_point(int x, int y, NativeColor& out, std::string& error);

// "#rrggbb", lowercase. Formatting only; no platform dependency.
inline std::string color_hex(const NativeColor& c) {
  static const char* kDigits = "0123456789abcdef";
  const unsigned char v[3] = {c.r, c.g, c.b};
  std::string s = "#";
  for (const unsigned char ch : v) {
    s += kDigits[ch >> 4];
    s += kDigits[ch & 0x0f];
  }
  return s;
}

// --- Active app menu (desktop only) ---
struct NativeMenuItem {
  std::string id;        // opaque, stable for this app/session
  std::string title;     // "Export as PNG"
  std::string path;      // "File > Export > PNG"
  std::string shortcut;  // "Ctrl+Shift+E" when the app reports one
  std::string app;       // owning application name
};

// Enumerates the focused application's menu tree through the platform
// accessibility API. Expensive (tens to hundreds of ms), so callers cache.
// Returns false with error set where no menu tree is exposed: mobile, and
// Linux until AT-SPI is wired up.
bool native_active_menu_items(std::vector<NativeMenuItem>& out, std::string& error);

// Runs a previously enumerated item. Fails when the menu changed shape since
// enumeration, which is expected when the user switched apps.
bool native_invoke_menu_item(const std::string& id, std::string& error);

// Records the app that is focused right now. Call this when the launcher is
// summoned: once the overlay takes focus, the "active app" is Wilfred itself,
// so menu search needs the app that was in front a moment earlier.
void native_menu_remember_foreground();

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
bool native_open_parent(const std::string& path);

}  // namespace wilfred
