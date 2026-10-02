#pragma once

#include "wilfred/config/config.hpp"

#include <functional>
#include <memory>

namespace wilfred {

using HotkeyFn = std::function<void()>;

class GlobalHotkey {
public:
  GlobalHotkey();
  ~GlobalHotkey();
  bool start(const Config& cfg, HotkeyFn cb);
  // One extra binding (cfg.hotkeys entries). Each call needs its own
  // GlobalHotkey instance; backends keep one registration per instance.
  bool start_binding(const std::vector<std::string>& modifiers, const std::string& key,
                     bool use_command_on_macos, HotkeyFn cb);
  void stop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace wilfred
