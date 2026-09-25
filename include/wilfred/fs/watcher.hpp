#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace wilfred {

enum class FsEventKind { Created, Deleted, Modified, Renamed, Overflow };

struct FsEvent {
  FsEventKind kind{FsEventKind::Modified};
  std::string path;
  std::string new_path;
};

using FsEventFn = std::function<void(const FsEvent&)>;

class FsWatcher {
public:
  FsWatcher();
  ~FsWatcher();
  bool start(const std::vector<std::string>& roots, int debounce_ms, FsEventFn cb);
  void stop();
  void add_root(const std::string& root);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace wilfred
