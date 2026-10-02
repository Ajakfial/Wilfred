#include "wilfred/search/quicknotes.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>

namespace wilfred {

QuickNoteStore& QuickNoteStore::instance() {
  static QuickNoteStore inst;
  return inst;
}

void QuickNoteStore::configure(std::string path) {
  std::lock_guard<std::mutex> lock(mu_);
  path_ = std::move(path);
}

bool QuickNoteStore::load() {
  std::string path;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
  }
  if (path.empty()) return false;
  std::string blob;
  if (!read_file_all(path, blob)) return false;
  std::lock_guard<std::mutex> lock(mu_);
  notes_.clear();
  std::string cur;
  for (char c : blob + "\n") {
    if (c == '\n') {
      if (!cur.empty() && cur.size() > 20) {
        // Format: "<id>\t<when>\t<text>"
        auto t1 = cur.find('\t');
        auto t2 = t1 == std::string::npos ? std::string::npos : cur.find('\t', t1 + 1);
        if (t1 != std::string::npos && t2 != std::string::npos) {
          QuickNote n;
          n.id = cur.substr(0, t1);
          try {
            n.when = std::stoll(cur.substr(t1 + 1, t2 - t1 - 1));
          } catch (...) {
            n.when = 0;
          }
          n.text = cur.substr(t2 + 1);
          notes_.push_back(std::move(n));
          try {
            int nid = std::stoi(n.id);
            if (nid >= next_) next_ = nid + 1;
          } catch (...) {
          }
        }
      }
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  return true;
}

bool QuickNoteStore::save_now() {
  std::string path;
  std::vector<QuickNote> copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
    copy = notes_;
  }
  if (path.empty()) return false;
  std::string blob;
  for (auto& n : copy) {
    std::string text = n.text;
    for (char& c : text)
      if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    blob += n.id + "\t" + std::to_string(n.when) + "\t" + text + "\n";
  }
  create_directories(path_parent(path));
  return write_file_atomic(path, blob.data(), blob.size());
}

std::string QuickNoteStore::add(const std::string& text) {
  std::string id;
  {
    std::lock_guard<std::mutex> lock(mu_);
    QuickNote n;
    n.id = std::to_string(next_++);
    n.text = text;
    n.when = unix_seconds();
    id = n.id;
    notes_.insert(notes_.begin(), std::move(n));
    if (notes_.size() > 500) notes_.resize(500);
  }
  save_now();
  return id;
}

bool QuickNoteStore::remove(const std::string& id_or_index) {
  bool removed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = notes_.begin(); it != notes_.end(); ++it) {
      if (it->id == id_or_index) {
        notes_.erase(it);
        removed = true;
        break;
      }
    }
    if (!removed) {
      try {
        int idx = std::stoi(id_or_index);
        if (idx >= 1 && idx <= static_cast<int>(notes_.size())) {
          notes_.erase(notes_.begin() + (idx - 1));
          removed = true;
        }
      } catch (...) {
      }
    }
  }
  if (removed) save_now();
  return removed;
}

void QuickNoteStore::clear() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    notes_.clear();
  }
  save_now();
}

std::vector<QuickNote> QuickNoteStore::list(const std::string& query, int limit) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<QuickNote> out;
  auto q = fold_search(normalize_query(query));
  for (auto& n : notes_) {
    if (limit > 0 && static_cast<int>(out.size()) >= limit) break;
    if (!q.empty() && fold_search(n.text).find(q) == std::string::npos) continue;
    out.push_back(n);
  }
  return out;
}

std::size_t QuickNoteStore::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return notes_.size();
}

// --- Todos ---

TodoStore& TodoStore::instance() {
  static TodoStore inst;
  return inst;
}

void TodoStore::configure(std::string path) {
  std::lock_guard<std::mutex> lock(mu_);
  path_ = std::move(path);
}

bool TodoStore::load() {
  std::string path;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
  }
  if (path.empty()) return false;
  std::string blob;
  if (!read_file_all(path, blob)) return false;
  std::lock_guard<std::mutex> lock(mu_);
  items_.clear();
  std::string cur;
  for (char c : blob + "\n") {
    if (c == '\n') {
      if (!cur.empty()) {
        // "<id>\t<done>\t<when>\t<text>"
        auto t1 = cur.find('\t');
        auto t2 = t1 == std::string::npos ? std::string::npos : cur.find('\t', t1 + 1);
        auto t3 = t2 == std::string::npos ? std::string::npos : cur.find('\t', t2 + 1);
        if (t1 != std::string::npos && t2 != std::string::npos && t3 != std::string::npos) {
          TodoItem t;
          try {
            t.id = std::stoi(cur.substr(0, t1));
            t.done = cur.substr(t1 + 1, t2 - t1 - 1) == "1";
            t.when = std::stoll(cur.substr(t2 + 1, t3 - t2 - 1));
          } catch (...) {
            cur.clear();
            continue;
          }
          t.text = cur.substr(t3 + 1);
          items_.push_back(std::move(t));
          if (t.id >= next_) next_ = t.id + 1;
        }
      }
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  return true;
}

bool TodoStore::save_now() {
  std::string path;
  std::vector<TodoItem> copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    path = path_;
    copy = items_;
  }
  if (path.empty()) return false;
  std::string blob;
  for (auto& t : copy) {
    std::string text = t.text;
    for (char& c : text)
      if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    blob += std::to_string(t.id) + "\t" + (t.done ? "1" : "0") + "\t" +
            std::to_string(t.when) + "\t" + text + "\n";
  }
  create_directories(path_parent(path));
  return write_file_atomic(path, blob.data(), blob.size());
}

std::string TodoStore::add(const std::string& text) {
  int id = 0;
  {
    std::lock_guard<std::mutex> lock(mu_);
    TodoItem t;
    t.id = next_++;
    t.text = text;
    t.when = unix_seconds();
    id = t.id;
    items_.push_back(std::move(t));
    if (items_.size() > 1000) items_.erase(items_.begin());
  }
  save_now();
  return std::to_string(id);
}

bool TodoStore::set_done(int id, bool done) {
  bool changed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& t : items_)
      if (t.id == id) {
        t.done = done;
        changed = true;
        break;
      }
  }
  if (changed) save_now();
  return changed;
}

bool TodoStore::remove(int id) {
  bool removed = false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = items_.begin(); it != items_.end(); ++it)
      if (it->id == id) {
        items_.erase(it);
        removed = true;
        break;
      }
  }
  if (removed) save_now();
  return removed;
}

void TodoStore::clear_done() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    items_.erase(std::remove_if(items_.begin(), items_.end(),
                                [](const TodoItem& t) { return t.done; }),
                 items_.end());
  }
  save_now();
}

void TodoStore::clear_all() {
  {
    std::lock_guard<std::mutex> lock(mu_);
    items_.clear();
  }
  save_now();
}

std::vector<TodoItem> TodoStore::list(bool include_done, const std::string& query,
                                      int limit) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<TodoItem> out;
  auto q = fold_search(normalize_query(query));
  // Open first, then done.
  for (int pass = 0; pass < 2; ++pass) {
    for (auto& t : items_) {
      if (limit > 0 && static_cast<int>(out.size()) >= limit) break;
      bool is_done = t.done;
      if (pass == 0 && is_done) continue;
      if (pass == 1 && !is_done) continue;
      if (!include_done && is_done) continue;
      if (!q.empty() && fold_search(t.text).find(q) == std::string::npos) continue;
      out.push_back(t);
    }
  }
  return out;
}

std::size_t TodoStore::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return items_.size();
}

std::size_t TodoStore::open_count() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t n = 0;
  for (auto& t : items_)
    if (!t.done) ++n;
  return n;
}

}  // namespace wilfred
