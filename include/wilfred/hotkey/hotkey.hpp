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
  void stop();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace wilfred
