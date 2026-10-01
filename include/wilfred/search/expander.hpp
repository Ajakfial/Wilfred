#pragma once

// Global snippet expansion: abbreviations typed in any app expand in place.
//
// When `snippets.global_expansion` is true the daemon watches keystrokes,
// matches the trailing token against SnippetStore triggers, and replaces it
// with the placeholder-expanded body (supports {date}, {time}, {clipboard}).
//
// Platform status:
//  - Windows: low-level keyboard hook (WH_KEYBOARD_LL) + SendInput paste.
//  - macOS/Linux: stub that keeps the API stable; expansion still works from
//    the Wilfred overlay (Enter pastes). A full CGEventTap / XInput hook is
//    left as an extension point so the core stays dependency-free.

#include <string>

namespace wilfred {

class SnippetStore;
struct Config;

class GlobalExpander {
 public:
  GlobalExpander() = default;
  ~GlobalExpander() { stop(); }

  GlobalExpander(const GlobalExpander&) = delete;
  GlobalExpander& operator=(const GlobalExpander&) = delete;

  bool start(const Config& cfg, SnippetStore* snippets);
  void stop();
  bool running() const { return running_; }

 private:
  bool running_{false};
  SnippetStore* snippets_{nullptr};
  const Config* cfg_{nullptr};
#ifdef _WIN32
  void* hook_{nullptr};
  void* thread_{nullptr};
  unsigned long thread_id_{0};
#endif
};

}  // namespace wilfred
