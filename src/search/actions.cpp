#include "wilfred/search/actions.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/search/clipboard.hpp"

#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>
#endif

namespace wilfred {

static PluginHost* g_plugin_exec = nullptr;

void set_plugin_host_for_actions(PluginHost* host) { g_plugin_exec = host; }

void attach_result_actions(SearchResult& r) {
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
  add("open", "Open");
  add("reveal", "Show in folder");
  add("copy_path", "Copy path");
  add("copy_name", "Copy name");
}

void attach_result_actions(std::vector<SearchResult>& results) {
  for (auto& r : results) attach_result_actions(r);
}

bool action_hides_overlay(const std::string& action_id) {
  if (action_id.empty() || action_id == "open" || action_id == "reveal" || action_id == "paste" ||
      action_id == "expand")
    return true;
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
  if (r.action == ResultAction::Plugin || r.category == "plugin") {
    if (g_plugin_exec && g_plugin_exec->execute(r, id)) return true;
  }
  if (r.action == ResultAction::WebSearch) return open_in_default_browser(r.payload);
  if (r.path.rfind("http://", 0) == 0 || r.path.rfind("https://", 0) == 0) return open_url(r.path);
  return launch_path(r.path.empty() ? r.payload : r.path);
}

}  // namespace wilfred
