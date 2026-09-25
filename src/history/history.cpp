#include "wilfred/history/history.hpp"

#include "wilfred/core/log.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>

namespace wilfred {

void HistoryStore::bump() { ++generation_; }

void HistoryStore::record_query(const std::string& query) {
  if (!enabled_ || query.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  queries_.erase(std::remove(queries_.begin(), queries_.end(), query), queries_.end());
  queries_.insert(queries_.begin(), query);
  if (static_cast<int>(queries_.size()) > max_) queries_.resize(static_cast<std::size_t>(max_));
  ++query_freq_[query];
  bump();
}

void HistoryStore::record_selection(const std::string& path) {
  if (!enabled_ || path.empty()) return;
  std::lock_guard<std::mutex> lock(mu_);
  ++freq_[path];
  last_[path] = unix_seconds();
  bump();
}

void HistoryStore::record_choice(const std::string& query, const std::string& path) {
  if (!enabled_ || path.empty()) return;
  std::string key = fold_search(query);
  if (key.empty()) key = fold_search(path);
  std::lock_guard<std::mutex> lock(mu_);
  ++choices_[key][path];
  bump();
}

void HistoryStore::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  queries_.clear();
  freq_.clear();
  last_.clear();
  query_freq_.clear();
  choices_.clear();
  bump();
}

int HistoryStore::frequency(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = freq_.find(path);
  return it == freq_.end() ? 0 : it->second;
}

int HistoryStore::query_frequency(const std::string& query) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = query_freq_.find(query);
  return it == query_freq_.end() ? 0 : it->second;
}

int HistoryStore::choice_count(const std::string& query, const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto qit = choices_.find(fold_search(query));
  if (qit == choices_.end()) return 0;
  auto pit = qit->second.find(path);
  return pit == qit->second.end() ? 0 : pit->second;
}

std::int64_t HistoryStore::last_selected(const std::string& path) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = last_.find(path);
  return it == last_.end() ? 0 : it->second;
}

std::vector<std::string> HistoryStore::suggest_queries(const std::string& prefix, int n) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto pre = fold_search(prefix);
  std::vector<std::pair<int, std::string>> scored;
  for (auto& [q, c] : query_freq_) {
    auto fq = fold_search(q);
    if (!pre.empty() && fq.find(pre) != 0 && fq.find(pre) == std::string::npos) continue;
    if (q == prefix) continue;
    scored.push_back({c, q});
  }
  if (scored.empty() && pre.empty()) {
    for (auto& q : queries_) scored.push_back({1, q});
  }
  std::sort(scored.begin(), scored.end(), [](auto& a, auto& b) {
    if (a.first != b.first) return a.first > b.first;
    return a.second.size() < b.second.size();
  });
  std::vector<std::string> out;
  for (auto& [c, q] : scored) {
    out.push_back(q);
    if (static_cast<int>(out.size()) >= n) break;
  }
  return out;
}

std::vector<std::pair<std::string, int>> HistoryStore::top_selections(int n) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<std::pair<std::string, int>> v(freq_.begin(), freq_.end());
  std::sort(v.begin(), v.end(), [](auto& a, auto& b) { return a.second > b.second; });
  if (static_cast<int>(v.size()) > n) v.resize(static_cast<std::size_t>(n));
  return v;
}

std::unordered_map<std::string, int> HistoryStore::choices_for(const std::string& query) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto key = fold_search(query);
  auto it = choices_.find(key);
  if (it != choices_.end()) return it->second;
  std::unordered_map<std::string, int> blended;
  if (key.size() < 2) return blended;
  for (auto& [q, paths] : choices_) {
    if (q.rfind(key, 0) == 0 || key.rfind(q, 0) == 0) {
      for (auto& [p, c] : paths) blended[p] += c;
    }
  }
  return blended;
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
  buf += "N\n";
  for (auto& [q, n] : query_freq_) {
    buf += q;
    buf += '\t';
    buf += std::to_string(n);
    buf += '\n';
  }
  buf += "C\n";
  for (auto& [q, paths] : choices_) {
    for (auto& [p, n] : paths) {
      buf += q;
      buf += '\t';
      buf += p;
      buf += '\t';
      buf += std::to_string(n);
      buf += '\n';
    }
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
  query_freq_.clear();
  choices_.clear();
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
        try {
          freq_[line.substr(0, t)] = std::stoi(line.substr(t + 1));
        } catch (...) {
          log_warn("history", "Malformed freq entry: " + line);
        }
      }
    } else if (mode == "L") {
      auto t = line.find('\t');
      if (t != std::string::npos) {
        try {
          last_[line.substr(0, t)] = std::stoll(line.substr(t + 1));
        } catch (...) {
          log_warn("history", "Malformed last entry: " + line);
        }
      }
    } else if (mode == "N") {
      auto t = line.find('\t');
      if (t != std::string::npos) {
        try {
          query_freq_[line.substr(0, t)] = std::stoi(line.substr(t + 1));
        } catch (...) {
          log_warn("history", "Malformed query-freq entry: " + line);
        }
      }
    } else if (mode == "C") {
      auto t1 = line.find('\t');
      auto t2 = t1 == std::string::npos ? std::string::npos : line.find('\t', t1 + 1);
      if (t1 != std::string::npos && t2 != std::string::npos) {
        try {
          choices_[line.substr(0, t1)][line.substr(t1 + 1, t2 - t1 - 1)] =
              std::stoi(line.substr(t2 + 1));
        } catch (...) {
          log_warn("history", "Malformed choice entry: " + line);
        }
      }
    }
    line.clear();
  };
  for (char c : text) {
    if (c == '\n') {
      if (line == "Q" || line == "F" || line == "L" || line == "N" || line == "C") {
        mode = line;
        line.clear();
      } else
        flush();
    } else
      line.push_back(c);
  }
  flush();
  bump();
  return true;
}

}  // namespace wilfred
