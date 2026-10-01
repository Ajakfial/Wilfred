#include "wilfred/search/actions.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/file_ops.hpp"
#include "wilfred/search/screenshot.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#endif

namespace wilfred {

static PluginHost* g_plugin_exec = nullptr;

void set_plugin_host_for_actions(PluginHost* host) { g_plugin_exec = host; }

namespace {

constexpr std::size_t kMaxOpenWithApps = 4;
constexpr std::size_t kMaxOpenWithResults = 8;

void add_file_actions(SearchResult& r, bool is_dir, bool include_open_with) {
  auto add = [&](const char* id, const std::string& label) {
    ResultActionItem it;
    it.id = id;
    it.label = label;
    r.actions.push_back(std::move(it));
  };
  auto p = r.path.empty() ? r.payload : r.path;
  add("open", "Open");
  add("reveal", "Show in folder");
  add("copy_path", "Copy path");
  add("copy_name", "Copy name");
  if (path_to_posix(p) != p) add("copy_posix", "Copy POSIX path");
  add("copy_file_uri", "Copy file URL");
#ifdef _WIN32
  if (!path_to_wsl(p).empty()) add("copy_wsl", "Copy WSL path");
#endif
  if (!is_dir) add("hash_file", "Copy SHA-256 hash");
  add("compress_zip", "Compress to .zip");
  add("open_terminal", "Open terminal here");
  add("open_editor", "Open in editor");
  if (is_dir) {
    add("new_file", "New file here");
    add("new_folder", "New folder here");
  }
  if (include_open_with && !is_dir) {
    for (auto& app : native_apps_for_file(p, kMaxOpenWithApps)) {
      ResultActionItem it;
      it.id = "open_with:" + app.target;
      it.label = "Open with " + app.name;
      r.actions.push_back(std::move(it));
    }
  }
}

void attach_impl(SearchResult& r, bool include_open_with) {
  if (!r.actions.empty()) return;
  if (r.action == ResultAction::Habit) return;
  auto add = [&](const char* id, const char* label) {
    ResultActionItem it;
    it.id = id;
    it.label = label;
    r.actions.push_back(std::move(it));
  };
  if (r.category == "snippet" || r.action == ResultAction::Expand) {
    add("paste", "Paste");
    add("copy_text", "Copy text");
    return;
  }
  if (r.action == ResultAction::Calculate || r.action == ResultAction::Convert ||
      r.action == ResultAction::Copy || r.action == ResultAction::Mini) {
    add("copy_text", "Copy");
    return;
  }
  if (r.action == ResultAction::System || r.category == "system") {
    auto sid = r.payload.empty() ? r.path : r.payload;
    const char* lab = "Run";
    if (sid == "lock") lab = "Lock";
    else if (sid == "sleep") lab = "Sleep";
    else if (sid == "shutdown") lab = "Shut down";
    else if (sid == "restart") lab = "Restart";
    else if (sid == "logout") lab = "Log out";
    else if (sid == "empty_trash") lab = "Empty";
    add("open", lab);
    return;
  }
  if (r.action == ResultAction::WebSearch || r.path.rfind("http://", 0) == 0 ||
      r.path.rfind("https://", 0) == 0) {
    add("open", "Open");
    add("copy_path", "Copy URL");
    return;
  }
  if (r.action == ResultAction::Plugin || r.category == "plugin") {
    add("open", "Run");
    add("copy_path", "Copy path");
    return;
  }
  if (r.action == ResultAction::SwitchWindow || r.category == "window") {
    add("open", "Switch");
    add("copy_name", "Copy title");
    return;
  }
  if (r.action == ResultAction::Screenshot || r.category == "screenshot") {
    add("open", "Capture");
    add("reveal", "Capture + show in folder");
    add("copy_path", "Capture + copy path");
    return;
  }
  if (r.kind == FileKind::Application) {
    add("open", "Open");
    add("reveal", "Show in folder");
    add("copy_path", "Copy path");
    add("copy_name", "Copy name");
    return;
  }
  add_file_actions(r, r.kind == FileKind::Directory, include_open_with);
}

}  // namespace

void attach_result_actions(SearchResult& r) { attach_impl(r, true); }

void attach_result_actions(std::vector<SearchResult>& results) {
  // Open-with lookups touch the registry / LaunchServices / .desktop files,
  // so only the first few file results per query get them.
  std::size_t budget = kMaxOpenWithResults;
  for (auto& r : results) {
    if (!r.actions.empty() || r.action == ResultAction::Habit) continue;
    bool include = true;
    if (r.kind != FileKind::Application && r.kind != FileKind::Directory &&
        r.action != ResultAction::Screenshot && r.category != "screenshot") {
      if (budget == 0)
        include = false;
      else
        --budget;
    }
    attach_impl(r, include);
  }
}

bool action_hides_overlay(const std::string& action_id) {
  if (action_id.empty() || action_id == "open" || action_id == "reveal" || action_id == "paste" ||
      action_id == "expand" || action_id == "open_terminal" || action_id == "open_editor" ||
      action_id == "compress_zip" || action_id == "new_file" || action_id == "new_folder")
    return true;
  if (action_id.rfind("open_with:", 0) == 0) return true;
  return false;
}

bool native_simulate_paste() {
#ifdef _WIN32
  INPUT in[4]{};
  in[0].type = INPUT_KEYBOARD;
  in[0].ki.wVk = VK_CONTROL;
  in[1].type = INPUT_KEYBOARD;
  in[1].ki.wVk = 'V';
  in[2].type = INPUT_KEYBOARD;
  in[2].ki.wVk = 'V';
  in[2].ki.dwFlags = KEYEVENTF_KEYUP;
  in[3].type = INPUT_KEYBOARD;
  in[3].ki.wVk = VK_CONTROL;
  in[3].ki.dwFlags = KEYEVENTF_KEYUP;
  return SendInput(4, in, sizeof(INPUT)) == 4;
#elif defined(__APPLE__)
  CGEventSourceRef src = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
  if (!src) return false;
  CGEventRef down = CGEventCreateKeyboardEvent(src, kVK_ANSI_V, true);
  CGEventRef up = CGEventCreateKeyboardEvent(src, kVK_ANSI_V, false);
  if (down && up) {
    CGEventSetFlags(down, kCGEventFlagMaskCommand);
    CGEventSetFlags(up, kCGEventFlagMaskCommand);
    CGEventPost(kCGHIDEventTap, down);
    CGEventPost(kCGHIDEventTap, up);
  }
  if (down) CFRelease(down);
  if (up) CFRelease(up);
  CFRelease(src);
  return true;
#else
  return false;
#endif
}

bool execute_result_action(const SearchResult& r, const Config& cfg, const std::string& action_id) {
  auto id = action_id;
  if (id.empty()) {
    if (r.action == ResultAction::Reveal) id = "reveal";
    else if (r.action == ResultAction::Copy || r.action == ResultAction::Mini ||
             r.action == ResultAction::Calculate || r.action == ResultAction::Convert)
      id = "copy_text";
    else if (r.action == ResultAction::Expand)
      id = "paste";
    else
      id = "open";
  }
  if (r.action == ResultAction::Screenshot || r.category == "screenshot") {
    ScreenshotMode mode = ScreenshotMode::Fullscreen;
    auto raw = r.payload.empty() ? r.path : r.payload;
    if (!parse_screenshot_mode(raw, mode)) parse_screenshot_payload(raw, mode);
    // Give the overlay a beat to hide so it is not in the capture.
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    std::string out_path;
    std::string err;
    if (!take_screenshot(mode, out_path, err)) {
      log_warn("screenshot", err.empty() ? "capture failed" : err);
      return false;
    }
    if (out_path.empty()) return true;  // delegated to an OS picker UI.
    if (id == "reveal") return reveal_path(out_path);
    if (id == "copy_path" || id == "copy_text" || id == "copy") return write_clipboard(out_path);
    if (id == "copy_name") return write_clipboard(out_path);
    return launch_path(out_path);
  }
  if (id == "reveal") {
    auto p = r.path.empty() ? r.payload : r.path;
    return reveal_path(p);
  }
  if (id == "copy_path") {
    auto p = r.path.empty() ? r.payload : r.path;
    return write_clipboard(p);
  }
  if (id == "copy_name") return write_clipboard(r.title);
  if (id == "copy_text" || id == "copy") {
    auto t = r.payload.empty() ? r.title : r.payload;
    return write_clipboard(t);
  }
  if (id == "paste" || id == "expand") {
    auto t = r.payload.empty() ? r.title : r.payload;
    if (!write_clipboard(t)) return false;
    if (cfg.snippets.auto_paste) {
      std::thread([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(90));
        native_simulate_paste();
      }).detach();
    }
    return true;
  }
  if (r.action == ResultAction::Habit || r.action == ResultAction::None) return false;
  if (r.action == ResultAction::Calculate || r.action == ResultAction::Convert) return true;
  {
    auto p = r.path.empty() ? r.payload : r.path;
    if (id == "copy_posix") return !p.empty() && write_clipboard(path_to_posix(p));
    if (id == "copy_file_uri") return !p.empty() && write_clipboard(path_to_file_uri(p));
    if (id == "copy_wsl") {
      auto w = path_to_wsl(p);
      if (w.empty()) return false;
      return write_clipboard(w);
    }
    if (id == "hash_file") {
      std::string hex, err;
      if (!sha256_file(p, hex, err)) {
        log_warn("fileops", err.empty() ? "hash failed" : err);
        return false;
      }
      return write_clipboard(hex);
    }
    if (id == "open_terminal" || id == "open_editor") {
      auto base = fs_is_directory(p) ? p : path_parent(p);
      if (base.empty() || !fs_exists(base)) return false;
      return id == "open_terminal" ? native_open_terminal(base) : native_open_editor(base);
    }
    if (id == "compress_zip") {
      if (p.empty() || !fs_exists(p)) return false;
      auto dir = path_parent(p);
      if (dir.empty()) return false;
      auto stem = path_stem(p);
      if (stem.empty()) stem = path_filename(p);
      if (stem.empty()) stem = "archive";
      auto dest = unique_sibling_path(dir, stem, ".zip");
      std::string err;
      if (!zip_paths_to({p}, dest, err)) {
        log_warn("fileops", err.empty() ? "compress failed" : err);
        return false;
      }
      reveal_path(dest);
      return true;
    }
    if (id == "new_file" || id == "new_folder") {
      auto base = fs_is_directory(p) ? p : path_parent(p);
      if (base.empty() || !fs_exists(base)) return false;
      std::string created, err;
      bool ok = id == "new_file" ? create_new_file_here(base, created, err)
                                 : create_new_folder_here(base, created, err);
      if (!ok) {
        log_warn("fileops", err.empty() ? "create failed" : err);
        return false;
      }
      reveal_path(created);
      return true;
    }
    if (id.rfind("open_with:", 0) == 0) {
      if (p.empty()) return false;
      return native_open_with(id.substr(10), p);
    }
  }
  if (r.action == ResultAction::System || r.category == "system") {
    auto sys_id = r.payload.empty() ? r.path : r.payload;
    return native_system_action(sys_id);
  }
  if (r.action == ResultAction::Plugin || r.category == "plugin") {
    if (g_plugin_exec && g_plugin_exec->execute(r, id)) return true;
  }
  if (r.action == ResultAction::WebSearch) return open_in_default_browser(r.payload);
  if (r.action == ResultAction::SwitchWindow || r.category == "window") {
    auto raw = r.payload.empty() ? r.path : r.payload;
    char* end = nullptr;
    auto wid = std::strtoull(raw.c_str(), &end, 10);
    if (!end || end == raw.c_str()) return false;
    return native_focus_window(static_cast<std::uint64_t>(wid));
  }
  if (r.path.rfind("http://", 0) == 0 || r.path.rfind("https://", 0) == 0) return open_url(r.path);
  return launch_path(r.path.empty() ? r.payload : r.path);
}

}  // namespace wilfred
