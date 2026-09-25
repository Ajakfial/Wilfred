#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace wilfred {

struct HistoryEvent {
  std::string query;
  std::string path;
  std::int64_t when{0};
};

class HistoryStore {
public:
  bool load(const std::string& path);
  bool save(const std::string& path) const;
  void record_query(const std::string& query);
  void record_selection(const std::string& path);
  void clear();
  int frequency(const std::string& path) const;
  std::int64_t last_selected(const std::string& path) const;
  const std::vector<std::string>& recent_queries() const { return queries_; }
  void set_max(int n) { max_ = n; }
  bool enabled() const { return enabled_; }
  void set_enabled(bool e) { enabled_ = e; }

  std::unordered_map<std::string, int> freq_map() const {
    std::lock_guard<std::mutex> lock(mu_);
    return freq_;
  }
  std::unordered_map<std::string, std::int64_t> last_map() const {
    std::lock_guard<std::mutex> lock(mu_);
    return last_;
  }

private:
  mutable std::mutex mu_;
  std::vector<std::string> queries_;
  std::unordered_map<std::string, int> freq_;
  std::unordered_map<std::string, std::int64_t> last_;
  int max_{8000};
  bool enabled_{true};
};

}  // namespace wilfred
