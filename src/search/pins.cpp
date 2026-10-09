#include "wilfred/search/pins.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/utf8.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace wilfred {

PinStore& PinStore::instance() {
  static PinStore inst;
  return inst;
}

std::string default_pins_path() {
  return path_join(data_directory(), "pins.bin");
}

void PinStore::configure(std::string path, const std::vector<std::string>& from_config) {
  std::lock_guard<std::mutex> lock(mu_);
  path_ = std::move(path);
  for (auto& p : from_config) {
    auto l = to_lower_utf8(p);
    if (l.empty()) continue;
    if (std::find(pins_.begin(), pins_.end(), l) == pins_.end()) pins_.push_back(l);
    if (pins_.size() >= 256) break;
  }
}

bool PinStore::load() {
  std::string path;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
    if (path.empty()) path = default_pins_path();
  }
  std::string blob;
  if (!read_file_all(path, blob) || blob.size() < 11) return false;
  if (blob.compare(0, 6, "WLPIN1") != 0) return false;
  std::size_t i = 6;
  auto take_u32 = [&](std::uint32_t& v) -> bool {
    if (i + 4 > blob.size()) return false;
    v = static_cast<std::uint8_t>(blob[i]) | (static_cast<std::uint32_t>(blob[i + 1]) << 8) |
        (static_cast<std::uint32_t>(blob[i + 2]) << 16) |
        (static_cast<std::uint32_t>(blob[i + 3]) << 24);
    i += 4;
    return true;
  };
  std::uint32_t n = 0;
  if (!take_u32(n) || n > 1000) return false;
  std::vector<std::string> out;
  for (std::uint32_t k = 0; k < n; ++k) {
    std::uint32_t len = 0;
    if (!take_u32(len) || i + len > blob.size() || len > 1024) return false;
    std::string s = blob.substr(i, len);
    i += len;
    if (!s.empty()) out.push_back(to_lower_utf8(s));
  }
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& s : out)
    if (std::find(pins_.begin(), pins_.end(), s) == pins_.end()) pins_.push_back(s);
  return true;
}

bool PinStore::save_now() {
  std::lock_guard<std::mutex> lock(mu_);
  std::string path = path_.empty() ? default_pins_path() : path_;
  std::string blob("WLPIN1", 6);
  auto put = [&](std::uint32_t v) {
    for (int b = 0; b < 4; ++b)
      blob.push_back(static_cast<char>((v >> (b * 8)) & 0xff));
  };
  put(static_cast<std::uint32_t>(pins_.size()));
  for (auto& p : pins_) {
    put(static_cast<std::uint32_t>(p.size()));
    blob.append(p);
  }
  create_directories(path_parent(path));
  return write_file_atomic(path, blob.data(), blob.size());
}

bool PinStore::add(const std::string& text) {
  auto l = to_lower_utf8(text);
  while (!l.empty() && (l.front() == ' '))
    l.erase(l.begin());
  while (!l.empty() && (l.back() == ' '))
    l.pop_back();
  if (l.empty()) return false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (std::find(pins_.begin(), pins_.end(), l) != pins_.end()) return false;
    if (pins_.size() >= 256) return false;
    pins_.push_back(l);
  }
  save_now();
  return true;
}

bool PinStore::remove(const std::string& text) {
  auto l = to_lower_utf8(text);
  bool removed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = std::find(pins_.begin(), pins_.end(), l);
    if (it != pins_.end()) {
      pins_.erase(it);
      removed = true;
    } else {
      // Substring removal: `unpin foo` removes any pin containing foo.
      for (auto it2 = pins_.begin(); it2 != pins_.end();) {
        if (it2->find(l) != std::string::npos || l.find(*it2) != std::string::npos) {
          it2 = pins_.erase(it2);
          removed = true;
        } else {
          ++it2;
        }
      }
    }
  }
  if (removed) save_now();
  return removed;
}

void PinStore::clear() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    pins_.clear();
  }
  save_now();
}

std::vector<std::string> PinStore::list() const {
  std::lock_guard<std::mutex> lock(mu_);
  return pins_;
}

bool PinStore::contains(const std::string& text) const {
  auto l = to_lower_utf8(text);
  std::lock_guard<std::mutex> lock(mu_);
  return std::find(pins_.begin(), pins_.end(), l) != pins_.end();
}

bool PinStore::matches(const std::string& title, const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  if (pins_.empty()) return false;
  auto hay = to_lower_utf8(title + " " + path);
  for (auto& p : pins_)
    if (!p.empty() && hay.find(p) != std::string::npos) return true;
  return false;
}

std::size_t PinStore::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return pins_.size();
}

}  // namespace wilfred
