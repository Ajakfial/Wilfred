#include "wilfred/history/history.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/time_util.hpp"

#include <algorithm>

namespace wilfred {

void HistoryStore::record_query(const std::string& query) {
  if (!enabled_ || query.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  queries_.erase(std::remove(queries_.begin(), queries_.end(), query), queries_.end());
  queries_.insert(queries_.begin(), query);
  if (static_cast<int>(queries_.size()) > max_) queries_.resize(static_cast<std::size_t>(max_));
}

void HistoryStore::record_selection(const std::string& path) {
  if (!enabled_ || path.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  ++freq_[path];
  last_[path] = unix_seconds();
}

void HistoryStore::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  queries_.clear();
  freq_.clear();
  last_.clear();
}

int HistoryStore::frequency(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = freq_.find(path);
  return it == freq_.end() ? 0 : it->second;
}

std::int64_t HistoryStore::last_selected(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = last_.find(path);
  return it == last_.end() ? 0 : it->second;
}

bool HistoryStore::save(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::string buf;
  buf += "Q\n";
  for (auto& q : queries_) {
    buf += q;
    buf += '\n';
  }
  buf += "F\n";
  for (auto& [p, n] : freq_) {
    buf += p;
    buf += '\t';
    buf += std::to_string(n);
    buf += '\n';
  }
  buf += "L\n";
  for (auto& [p, t] : last_) {
    buf += p;
    buf += '\t';
    buf += std::to_string(t);
    buf += '\n';
  }
  return write_file_atomic(path, buf.data(), buf.size());
}

bool HistoryStore::load(const std::string& path) {
  std::string text;
  if (!read_file_all(path, text)) return false;
  std::lock_guard<std::mutex> lock(mu_);
  queries_.clear();
  freq_.clear();
  last_.clear();
  std::string mode;
  std::string line;
  auto flush = [&] {
    while (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) return;
    if (mode == "Q")
      queries_.push_back(line);
    else if (mode == "F") {
      auto t = line.find('\t');
      if (t != std::string::npos) {
        try { freq_[line.substr(0, t)] = std::stoi(line.substr(t + 1)); }
        catch (...) { log_warn("history", "Malformed freq entry: " + line); }
      }
    } else if (mode == "L") {
      auto t = line.find('\t');
      if (t != std::string::npos) {
        try { last_[line.substr(0, t)] = std::stoll(line.substr(t + 1)); }
        catch (...) { log_warn("history", "Malformed last entry: " + line); }
      }
    }
    line.clear();
  };
  for (char c : text) {
    if (c == '\n') {
      if (line == "Q" || line == "F" || line == "L") {
        mode = line;
        line.clear();
      } else
        flush();
    } else
      line.push_back(c);
  }
  flush();
  return true;
}

}  // namespace wilfred
