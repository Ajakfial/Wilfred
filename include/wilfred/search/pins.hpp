#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

// Persistent pinned favorites backing the `pin`/`pins` minis and the
// `ranking.pinned` boost. Entries are lowercased title/path substrings.
class PinStore {
 public:
  static PinStore& instance();

  void configure(std::string path, const std::vector<std::string>& from_config);
  bool load();
  bool save_now();

  bool add(const std::string& text);
  bool remove(const std::string& text);
  void clear();
  std::vector<std::string> list() const;
  bool matches(const std::string& title, const std::string& path) const;
  bool contains(const std::string& text) const;
  std::size_t size() const;

 private:
  PinStore() = default;
  mutable std::mutex mu_;
  std::vector<std::string> pins_;
  std::string path_;
};

std::string default_pins_path();

}  // namespace wilfred
