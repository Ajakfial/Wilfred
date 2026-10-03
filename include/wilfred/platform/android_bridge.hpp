#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace wilfred {

// Portable mobile core: no JNI/ObjC types here so desktop tests can exercise
// it. The Android JNI layer (android/app/src/main/cpp/wilfred_jni.cpp) and
// the iOS ObjC++ bridge (ios/Wilfred/WilfredCoreBridge.mm) are thin
// mutex-guarded wrappers around MobileCore, and the Kotlin/Swift UI owns all
// platform actions (app launch, URL open, file open).
// UI lives entirely in Kotlin (Android: MainActivity + FloatingWService
// popup) or Swift (iOS: ContentView); all search/index/rank/calc/snippets/
// notes/todos/timers/clips/macros logic lives in C++ (wilfred_core) via
// this bridge.
class MobileCore {
 public:
  MobileCore();
  ~MobileCore();

  // files_dir must be the app-private files directory
  // (Android Context.getFilesDir(), iOS app sandbox documents dir).
  // Config/index/history/snippets all live under it so no other permission
  // is needed for the core to run.
  bool boot(const std::string& files_dir, std::string& error);

  // JSON array of {title,subtitle,path,payload,score,action,category,kind,
  // actions:[{id,label}]}. Never throws; returns "[]" on any failure.
  // Also snapshots the results so execute_action()/actions_json() can act
  // on them by index without re-querying.
  std::string search_json(const std::string& query, int limit);

  std::string status_json();
  bool index_now(std::string& error);

  // Installed packages enumerated in Kotlin (PackageManager). Stored as
  // application records so they rank like desktop apps. (iOS has no app
  // enumeration API; the Swift layer never calls this.)
  bool register_app(const std::string& name, const std::string& package_id,
                    const std::string& label);
  bool record_choice(const std::string& query, const std::string& key);

  // ---- Full-functionality bridge (all served from C++) ----
  // Push the current OS clipboard text into the C++ core so clipboard
  // minis, snippet `{clipboard}` expansion and path hints work.
  void set_clipboard(const std::string& text);
  // Current clipboard text known to the core (override or last copy).
  std::string clipboard_text();

  // Assist payload: {"correction":"","ghost":"","candidates":[]}.
  // Powers "did you mean", ghost completion and the candidate strip.
  std::string assist_json(const std::string& query);

  // Actions for a result from the last search_json() call:
  // [{"id":"open","label":"Open"},...]. Returns "[]" on bad index.
  std::string actions_json(std::size_t result_index);

  // Run a C++ side-effect action (timer_stop, note_delete:*, todo_done:*,
  // clip_pin, transcribe_run, copy_*, etc.) on a stored last-search result.
  // Returns true on success. Copy-type actions also update the core
  // clipboard; the Kotlin/Swift layer should copy `payload`/`path` to the
  // OS clipboard.
  bool execute_action(std::size_t result_index, const std::string& action_id,
                      std::string& error);

  // File preview for the F3-style peek UI:
  // {"exists":true,"is_dir":false,"size":123,"name":"x","preview":"..."}.
  std::string preview_json(const std::string& path);

  static std::string results_to_json(
      const std::vector<struct SearchResult>& results, std::size_t limit);
  static std::string action_to_string(int action_value);

 private:
  struct Impl;
  Impl* impl_{nullptr};
};

// Pre-rename name kept for the JNI layer, tests and docs.
using AndroidCore = MobileCore;

}  // namespace wilfred
