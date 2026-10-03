#include "wilfred/search/actions.hpp"

#include "wilfred/apps/discovery.hpp"
#include "wilfred/browser/browser.hpp"
#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/platform/platform.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/clip_history.hpp"
#include "wilfred/search/file_ops.hpp"
#include "wilfred/search/layouts.hpp"
#include "wilfred/search/media.hpp"
#include "wilfred/search/quicknotes.hpp"
#include "wilfred/search/screenshot.hpp"
#include "wilfred/search/timers.hpp"
#include "wilfred/search/transcribe.hpp"

#include <chrono>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__) && !defined(WILFRED_IOS)
// ApplicationServices/Carbon exist on macOS only; iOS falls through to
// the no-op paste below (the Swift layer owns the pasteboard).
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
    add("window_minimize", "Minimize");
    add("window_maximize", "Maximize");
    add("window_restore", "Restore");
    add("window_close", "Close");
    add("window_snap_left", "Snap left");
    add("window_snap_right", "Snap right");
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

namespace {

std::string lower_copy(const std::string& s) {
  std::string o;
  o.reserve(s.size());
  for (unsigned char c : s) o.push_back(static_cast<char>(std::tolower(c)));
  return o;
}

std::string action_label_for(const std::string& id) {
  if (id == "open") return "Open";
  if (id == "reveal") return "Show in folder";
  if (id == "copy_path") return "Copy path";
  if (id == "copy_name") return "Copy name";
  if (id == "copy_posix") return "Copy POSIX path";
  if (id == "copy_file_uri") return "Copy file URL";
  if (id == "copy_wsl") return "Copy WSL path";
  if (id == "copy_text") return "Copy";
  if (id == "hash_file") return "Copy SHA-256 hash";
  if (id == "compress_zip") return "Compress to .zip";
  if (id == "open_terminal") return "Open terminal here";
  if (id == "open_editor") return "Open in editor";
  if (id == "new_file") return "New file here";
  if (id == "new_folder") return "New folder here";
  if (id == "kill_process") return "Kill process";
  if (id == "timer_stop") return "Stop timer";
  if (id == "transcribe_run") return "Transcribe";
  if (id.rfind("dictate_run", 0) == 0) return "Dictate";
  if (id.rfind("layout_apply:", 0) == 0) return "Apply layout";
  if (id.rfind("focus_window:", 0) == 0) return "Focus window";
  if (id == "window_minimize") return "Minimize";
  if (id == "window_maximize") return "Maximize";
  if (id == "window_restore") return "Restore";
  if (id == "window_close") return "Close";
  if (id == "window_snap_left") return "Snap left";
  if (id == "window_snap_right") return "Snap right";
  if (id == "layout_apply") return "Apply layout";
  return id;
}

bool is_file_like(const SearchResult& r) {
  if (r.action == ResultAction::Screenshot || r.category == "screenshot") return false;
  if (r.action == ResultAction::SwitchWindow || r.category == "window") return false;
  if (r.action == ResultAction::System || r.category == "system") return false;
  if (r.action == ResultAction::Calculate || r.action == ResultAction::Convert) return false;
  if (r.category == "timer" || r.category == "stopwatch" || r.category == "note" ||
      r.category == "todo" || r.category == "kill" || r.category == "media" ||
      r.category == "workflow" || r.category == "quicklink" || r.category == "process" ||
      r.category == "ping" || r.category == "dns" || r.category == "myip" ||
      r.category == "dupe" || r.category == "large" || r.category == "transcribe" ||
      r.category == "dictate" || r.category == "layout")
    return false;
  return true;
}

void append_context_actions(SearchResult& r, const Config& cfg) {
  if (!is_file_like(r) && r.kind != FileKind::Application) {
    // Workflows still apply to process/media? No — only file-like + apps.
    // But allow workflow on any actionable file result; skip pure text cards.
    if (!is_file_like(r)) return;
  }
  // Per-app extras: match lowercased app substring against title + path.
  if (!cfg.app_actions.empty()) {
    std::string hay = lower_copy(r.title + " " + r.path + " " + r.payload);
    for (auto& [app, ids] : cfg.app_actions) {
      if (app.empty()) continue;
      if (hay.find(app) == std::string::npos) continue;
      for (auto& aid : ids) {
        bool dup = false;
        for (auto& a : r.actions)
          if (a.id == aid) {
            dup = true;
            break;
          }
        if (dup) continue;
        ResultActionItem it;
        it.id = aid;
        it.label = action_label_for(aid) + " · " + app;
        r.actions.push_back(std::move(it));
      }
    }
  }
  // Named workflows as extra actions on file results.
  if (!cfg.workflows.empty() && is_file_like(r)) {
    for (auto& [name, steps] : cfg.workflows) {
      std::string wid = "workflow:" + name;
      bool dup = false;
      for (auto& a : r.actions)
        if (a.id == wid) {
          dup = true;
          break;
        }
      if (dup) continue;
      ResultActionItem it;
      it.id = wid;
      it.label = "Run " + name;
      r.actions.push_back(std::move(it));
    }
  }
}

}  // namespace

void attach_result_actions(SearchResult& r, const Config& cfg) {
  if (r.actions.empty() && r.action != ResultAction::Habit) attach_impl(r, true);
  append_context_actions(r, cfg);
}

void attach_result_actions(std::vector<SearchResult>& results, const Config& cfg) {
  attach_result_actions(results);
  for (auto& r : results) {
    if (r.action == ResultAction::Habit) continue;
    append_context_actions(r, cfg);
  }
}

bool action_hides_overlay(const std::string& action_id) {
  if (action_id.empty() || action_id == "open" || action_id == "reveal" || action_id == "paste" ||
      action_id == "expand" || action_id == "open_terminal" || action_id == "open_editor" ||
      action_id == "compress_zip" || action_id == "new_file" || action_id == "new_folder")
    return true;
  if (action_id.rfind("open_with:", 0) == 0) return true;
  if (action_id.rfind("workflow:", 0) == 0) return true;
  if (action_id == "kill_process" || action_id.rfind("media:", 0) == 0) return true;
  if (action_id.rfind("window_", 0) == 0) return true;
  if (action_id == "transcribe_run") return true;
  if (action_id.rfind("dictate_run", 0) == 0) return true;
  if (action_id.rfind("layout_apply:", 0) == 0) return true;
  if (action_id.rfind("focus_window:", 0) == 0) return true;
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
#elif defined(__APPLE__) && !defined(WILFRED_IOS)
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
    // Process kill cards default to killing; media cards run their action.
    if (r.category == "process" || r.category == "kill") {
      // `process` list cards: copy by default, `kill` cards: kill by default.
      if (r.category == "kill") id = "kill_process";
      else if (r.action == ResultAction::Copy || r.action == ResultAction::Mini)
        id = "copy_text";
      else
        id = "open";
      // When the empty action comes from overlay Enter on a kill card with an
      // explicit kill action attached, prefer killing.
      if (r.category == "kill") id = "kill_process";
    } else if (r.category == "transcribe") {
      id = "transcribe_run";
    } else if (r.payload.rfind("layout_apply:", 0) == 0) {
      id = r.payload;
    } else if (r.category == "media" && r.payload.rfind("media:", 0) == 0) {
      id = r.payload.substr(6);
      if (id.empty()) id = "copy_text";
      // Fall through to media handling below via id.
      if (id != "copy_text") {
        std::string err;
        if (native_media_action(id, err)) return true;
        log_warn("media", err.empty() ? "media action failed" : err);
        return false;
      }
    } else if (r.action == ResultAction::Reveal)
      id = "reveal";
    else if (r.action == ResultAction::Copy || r.action == ResultAction::Mini ||
             r.action == ResultAction::Calculate || r.action == ResultAction::Convert)
      id = "copy_text";
    else if (r.action == ResultAction::Expand)
      id = "paste";
    else
      id = "open";
  }
  // Named workflows: "workflow:<name>" expands to its '+'-joined chain.
  if (id.rfind("workflow:", 0) == 0) {
    auto name = id.substr(9);
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto it = cfg.workflows.find(name);
    if (it == cfg.workflows.end()) {
      log_warn("workflow", "unknown workflow '" + name + "'");
      return false;
    }
    std::string chain;
    for (std::size_t i = 0; i < it->second.size(); ++i) {
      if (i) chain += "+";
      chain += it->second[i];
    }
    if (chain.empty()) return false;
    return execute_result_action(r, cfg, chain);
  }
  // Workflow chaining: "copy_path+reveal" runs each step in order and
  // reports success only if every step succeeds. open_with targets may
  // contain '+' themselves, so they are never split.
  if (id.rfind("open_with:", 0) != 0 && id.find('+') != std::string::npos) {
    bool ok = true;
    std::size_t pos = 0;
    while (pos <= id.size()) {
      auto plus = id.find('+', pos);
      std::string seg =
          plus == std::string::npos ? id.substr(pos) : id.substr(pos, plus - pos);
      if (seg.empty() || !execute_result_action(r, cfg, seg)) ok = false;
      if (plus == std::string::npos) break;
      pos = plus + 1;
    }
    return ok;
  }
  if (id == "clip_pin" || id == "clip_unpin" || id == "clip_clear" ||
      (r.category == "clips" &&
       (r.payload == "clip:clear" || r.path == "clip:clear"))) {
    auto& store = ClipStore::instance();
    if (id == "clip_pin") {
      auto t = r.payload.empty() ? r.title : r.payload;
      return store.pin_text(t);
    }
    if (id == "clip_unpin") {
      auto t = r.payload.empty() ? r.title : r.payload;
      return store.unpin_text(t);
    }
    store.clear_unpinned();
    return true;
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
  // Process killer (works for both `process` list cards via r.id and `kill` cards).
  if (id == "kill_process" || (r.category == "kill" && id == "open")) {
    std::uint32_t pid = r.id;
    if (!pid) {
      // Fall back to parsing "pid:<n>" payloads.
      auto raw = r.payload.empty() ? r.path : r.payload;
      auto pos = raw.find("pid:");
      if (pos != std::string::npos) {
        try {
          pid = static_cast<std::uint32_t>(std::stoul(raw.substr(pos + 4)));
        } catch (...) {
          pid = 0;
        }
      }
    }
    if (!pid) {
      log_warn("kill", "no pid to kill");
      return false;
    }
    std::string err;
    if (!native_kill_process(pid, err)) {
      log_warn("kill", err.empty() ? "kill failed" : err);
      return false;
    }
    return true;
  }
  // Audio transcription: Enter on a transcribe card runs it; the transcript
  // lands on the clipboard (plus an optional sidecar) since the overlay has
  // no channel for long async text.
  if (id == "transcribe_run" || (r.category == "transcribe" && id == "open")) {
    auto audio = r.path.empty() ? r.payload : r.path;
    if (audio.empty() || !is_transcribe_candidate(audio)) {
      log_warn("transcribe", "no audio file to transcribe");
      return false;
    }
    std::string text, err;
    if (!transcribe_audio_file(audio, cfg, text, err)) {
      log_warn("transcribe", err.empty() ? "transcription failed" : err);
      return false;
    }
    if (!write_clipboard(text)) {
      log_warn("transcribe", "transcribed but clipboard write failed");
      return false;
    }
    if (cfg.transcription.save_txt) {
      std::string save_err;
      if (!write_transcript_sidecar(audio, text, save_err))
        log_warn("transcribe", save_err.empty() ? "sidecar save failed" : save_err);
    }
    return true;
  }
  // Window management on window cards (payload holds the numeric window id).
  if (id.rfind("window_", 0) == 0 && (r.action == ResultAction::SwitchWindow || r.category == "window")) {
    auto raw = r.payload.empty() ? r.path : r.payload;
    char* end = nullptr;
    auto wid = std::strtoull(raw.c_str(), &end, 10);
    if (!end || end == raw.c_str() || *end != '\0') {
      log_warn("window", "no window id to act on");
      return false;
    }
    auto op = id.substr(7);
    NativeWindowOp wop = NativeWindowOp::Restore;
    if (op == "minimize")
      wop = NativeWindowOp::Minimize;
    else if (op == "maximize")
      wop = NativeWindowOp::Maximize;
    else if (op == "restore")
      wop = NativeWindowOp::Restore;
    else if (op == "close")
      wop = NativeWindowOp::Close;
    else if (op == "snap_left")
      wop = NativeWindowOp::SnapLeft;
    else if (op == "snap_right")
      wop = NativeWindowOp::SnapRight;
    else {
      log_warn("window", "unknown window action '" + id + "'");
      return false;
    }
    std::string err;
    if (!native_window_action(static_cast<std::uint64_t>(wid), wop, err)) {
      log_warn("window", err.empty() ? "window action failed" : err);
      return false;
    }
    return true;
  }
  // Built-in dictation: record the mic, transcribe, clipboard the text.
  // Dictated audio is transient (data-dir WAV, deleted after), so unlike
  // file transcription there is no sidecar — the clipboard is the delivery.
  if (id.rfind("dictate_run", 0) == 0) {
    int seconds = 10;
    if (id.size() > 12 && id[12] == ':') {
      try {
        seconds = std::stoi(id.substr(13));
      } catch (...) {
        seconds = 10;
      }
      if (seconds < 1) seconds = 1;
      if (seconds > 120) seconds = 120;
    }
    if (!cfg.transcription.enabled) {
      log_warn("dictate", "transcription is disabled");
      return false;
    }
    create_directories(data_directory());
    auto wav = path_join(data_directory(), "transcribe-mic.wav");
    std::string err;
    if (!record_microphone(wav, seconds, cfg.transcription.mic, err)) {
      log_warn("dictate", err.empty() ? "recording failed" : err);
      return false;
    }
    std::string text;
    if (!transcribe_audio_file(wav, cfg, text, err)) {
      remove_file(wav);
      log_warn("dictate", err.empty() ? "transcription failed" : err);
      return false;
    }
    remove_file(wav);
    if (!write_clipboard(text)) {
      log_warn("dictate", "transcribed but clipboard write failed");
      return false;
    }
    return true;
  }
  // Window layouts + focus-by-name (automation-friendly window steps).
  if (id.rfind("layout_apply:", 0) == 0 || id.rfind("focus_window:", 0) == 0) {
    bool focus_only = id.rfind("focus_window:", 0) == 0;
    auto name = to_lower_utf8(id.substr(id.find(':') + 1));
    if (focus_only) {
      if (name.empty()) {
        log_warn("window", "no window name to focus");
        return false;
      }
      for (auto& w : native_list_windows()) {
        auto hay = to_lower_utf8(w.title + " " + w.owner);
        if (hay.find(name) != std::string::npos) {
          std::string err;
          if (!native_focus_window(w.id)) {
            log_warn("window", "could not focus window");
            return false;
          }
          return true;
        }
      }
      log_warn("window", "no window matching \"" + name + "\"");
      return false;
    }
    Layout lay;
    std::string err;
    if (!LayoutStore::instance().load_layout(name, lay, err)) {
      log_warn("layout", err.empty() ? "unknown layout" : err);
      return false;
    }
    auto wins = native_list_windows();
    std::vector<std::uint64_t> used;
    for (auto& e : lay.entries) {
      auto needle = to_lower_utf8(e.match);
      for (auto& w : wins) {
        bool taken = false;
        for (auto u : used)
          if (u == w.id) {
            taken = true;
            break;
          }
        if (taken) continue;
        auto hay = to_lower_utf8(w.title + " " + w.owner);
        if (hay.find(needle) == std::string::npos) continue;
        used.push_back(w.id);
        if (e.maximized) {
          if (!native_window_action(w.id, NativeWindowOp::Maximize, err))
            log_warn("layout", err.empty() ? "maximize failed" : err);
        } else if (!native_window_move(w.id, e.x, e.y, e.w, e.h, err)) {
          log_warn("layout", err.empty() ? "move failed" : err);
        }
        break;
      }
    }
    return true;
  }
  // Media controls on media cards or explicit media:<id> actions anywhere.
  if (id.rfind("media:", 0) == 0) {
    auto mid = id.substr(6);
    std::string err;
    if (!native_media_action(mid, err)) {
      log_warn("media", err.empty() ? "media action failed" : err);
      return false;
    }
    return true;
  }
  if (r.category == "media" && (id == "open" || id == "copy_text")) {
    // Media list cards carry payload "media:<id>"; running beats copying.
    auto raw = r.payload.empty() ? r.path : r.payload;
    if (raw.rfind("media:", 0) == 0) {
      if (id == "open") return execute_result_action(r, cfg, raw);
      // copy_text falls through to generic copy below.
    }
  }
  // Timers / notes / todos maintenance actions.
  if (id == "timer_stop") {
    TimerStore::instance().stop("");
    return true;
  }
  if (id.rfind("note_delete:", 0) == 0) {
    return QuickNoteStore::instance().remove(id.substr(12));
  }
  if (id.rfind("todo_done:", 0) == 0) {
    try {
      return TodoStore::instance().set_done(std::stoi(id.substr(10)), true);
    } catch (...) {
      return false;
    }
  }
  if (id.rfind("todo_undo:", 0) == 0) {
    try {
      return TodoStore::instance().set_done(std::stoi(id.substr(10)), false);
    } catch (...) {
      return false;
    }
  }
  if (id.rfind("todo_delete:", 0) == 0) {
    try {
      return TodoStore::instance().remove(std::stoi(id.substr(12)));
    } catch (...) {
      return false;
    }
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
