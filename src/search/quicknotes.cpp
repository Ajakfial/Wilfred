#include "wilfred/search/quicknotes.hpp"

#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/core/time_util.hpp"
#include "wilfred/core/utf8.hpp"
#include "wilfred/index/tokenizer.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <vector>

namespace wilfred {

namespace fs = std::filesystem;
namespace {

constexpr const char* kNoteFooterPrefix = "<!-- wilfred:note ";

std::string note_file(const std::string& dir, const std::string& id) {
  return path_join(dir, id + ".md");
}

std::string note_title(const std::string& text) {
  auto nl = text.find('\n');
  std::string t = nl == std::string::npos ? text : text.substr(0, nl);
  while (!t.empty() && (t.back() == ' ' || t.back() == '\t' || t.back() == '\r')) t.pop_back();
  while (!t.empty() && (t.front() == ' ' || t.front() == '\t')) t.erase(t.begin());
  if (t.size() > 80) t.resize(80);
  return t.empty() ? std::string("Note") : t;
}

std::string render_note(const QuickNote& n) {
  return "# " + note_title(n.text) + "\n\n" + n.text + "\n\n" + kNoteFooterPrefix +
         "id=" + n.id + " created=" + std::to_string(n.when) + " -->\n";
}

// Split blob into lines without the trailing newline artifacts.
std::vector<std::string> split_lines(const std::string& blob) {
  std::vector<std::string> lines;
  std::string cur;
  for (char c : blob) {
    if (c == '\n') {
      if (!cur.empty() && cur.back() == '\r') cur.pop_back();
      lines.push_back(cur);
      cur.clear();
    } else {
      cur.push_back(c);
    }
  }
  if (!cur.empty()) {
    if (cur.back() == '\r') cur.pop_back();
    lines.push_back(cur);
  }
  return lines;
}

// Parse one of our Markdown note files. Only files carrying the footer
// marker count as managed notes; anything else in the directory is left
// alone (it still shows up through the notes search provider).
bool parse_note_file(const std::string& stem, const std::string& blob, QuickNote& out) {
  auto lines = split_lines(blob);
  while (!lines.empty() && lines.back().empty()) lines.pop_back();
  if (lines.empty()) return false;
  auto& last = lines.back();
  if (last.rfind(kNoteFooterPrefix, 0) != 0 || last.size() < 4 ||
      last.compare(last.size() - 3, 3, "-->") != 0)
    return false;
  QuickNote n;
  n.id = stem;
  n.when = 0;
  auto id_pos = last.find("id=");
  if (id_pos != std::string::npos) {
    auto id_end = last.find_first_of(" >", id_pos + 3);
    auto fid = last.substr(id_pos + 3, id_end == std::string::npos ? std::string::npos
                                                                  : id_end - id_pos - 3);
    if (!fid.empty()) n.id = fid;
  }
  auto created_pos = last.find("created=");
  if (created_pos != std::string::npos) {
    try {
      n.when = std::stoll(last.substr(created_pos + 8));
    } catch (...) {
      n.when = 0;
    }
  }
  lines.pop_back();
  while (!lines.empty() && lines.back().empty()) lines.pop_back();
  if (!lines.empty() && lines.front().rfind("# ", 0) == 0) lines.erase(lines.begin());
  while (!lines.empty() && lines.front().empty()) lines.erase(lines.begin());
  std::string text;
  for (auto& ln : lines) {
    if (!text.empty()) text.push_back('\n');
    text += ln;
  }
  n.text = std::move(text);
  out = std::move(n);
  return true;
}

bool write_note_file(const std::string& dir, const QuickNote& n) {
  create_directories(dir);
  auto body = render_note(n);
  return write_file_atomic(note_file(dir, n.id), body.data(), body.size());
}

}  // namespace

QuickNoteStore& QuickNoteStore::instance() {
  static QuickNoteStore inst;
  return inst;
}

void QuickNoteStore::configure(std::string dir) {
  std::lock_guard<std::mutex> lock(mu_);
  dir_ = std::move(dir);
}

bool QuickNoteStore::load() {
  std::string dir;
  {
    std::lock_guard<std::mutex> lock(mu_);
    dir = dir_;
  }
  if (dir.empty()) return false;
  create_directories(dir);
  std::vector<QuickNote> notes;
  int next = 1;
  std::error_code ec;
  for (const auto& de : fs::directory_iterator(fs::u8path(dir), ec)) {
    if (ec) break;
    std::error_code ec2;
    if (!de.is_regular_file(ec2)) continue;
    auto p = de.path();
    auto ext = p.extension().u8string();
    std::string exts(ext.begin(), ext.end());
    if (to_lower_utf8(exts) != ".md") continue;
    auto stem_u8 = p.stem().u8string();
    std::string stem(stem_u8.begin(), stem_u8.end());
    std::string blob;
    auto full_u8 = p.u8string();
    if (!read_file_all(std::string(full_u8.begin(), full_u8.end()), blob)) continue;
    QuickNote n;
    if (!parse_note_file(stem, blob, n)) continue;  // not ours; leave it alone
    notes.push_back(std::move(n));
    try {
      int nid = std::stoi(notes.back().id);
      if (nid >= next) next = nid + 1;
    } catch (...) {
    }
  }
  // One-time import of the legacy quicknotes.txt next to the notes dir.
  {
    auto legacy_u8 = fs::u8path(path_join(path_parent(dir), "quicknotes.txt"));
    std::error_code ec3;
    if (fs::exists(legacy_u8, ec3)) {
      std::string legacy(legacy_u8.u8string().begin(), legacy_u8.u8string().end());
      std::string blob;
      if (read_file_all(legacy, blob)) {
        std::string cur;
        for (char c : blob + "\n") {
          if (c == '\n') {
            if (cur.size() > 20) {
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
                if (n.when == 0) n.when = unix_seconds();
                n.text = cur.substr(t2 + 1);
                bool dup = false;
                for (auto& e : notes)
                  if (e.text == n.text) {
                    dup = true;
                    break;
                  }
                if (!dup) {
                  try {
                    int nid = std::stoi(n.id);
                    if (nid >= next) {
                      // Keep the legacy id when free.
                      bool taken = false;
                      for (auto& e : notes)
                        if (e.id == n.id) {
                          taken = true;
                          break;
                        }
                      if (!taken) {
                        next = nid + 1;
                        notes.push_back(n);
                        write_note_file(dir, n);
                        cur.clear();
                        continue;
                      }
                    }
                  } catch (...) {
                  }
                  n.id = std::to_string(next++);
                  notes.push_back(n);
                  write_note_file(dir, n);
                }
              }
            }
            cur.clear();
          } else {
            cur.push_back(c);
          }
        }
      }
      std::error_code ec4;
      fs::rename(legacy_u8, fs::u8path(legacy + ".imported"), ec4);
    }
  }
  std::sort(notes.begin(), notes.end(), [](const QuickNote& a, const QuickNote& b) {
    if (a.when != b.when) return a.when > b.when;
    return a.id > b.id;
  });
  std::lock_guard<std::mutex> lock(mu_);
  if (dir_ != dir) return false;  // reconfigured mid-load; drop the read
  notes_ = std::move(notes);
  next_ = next;
  return true;
}

bool QuickNoteStore::save_now() {
  std::string dir;
  std::vector<QuickNote> copy;
  {
    std::lock_guard<std::mutex> lock(mu_);
    dir = dir_;
    copy = notes_;
  }
  if (dir.empty()) return false;
  create_directories(dir);
  for (auto& n : copy)
    if (!write_note_file(dir, n)) return false;
  return true;
}

std::string QuickNoteStore::add(const std::string& text) {
  std::string id;
  std::string dir;
  {
    std::lock_guard<std::mutex> lock(mu_);
    QuickNote n;
    n.id = std::to_string(next_++);
    n.text = text;
    n.when = unix_seconds();
    id = n.id;
    notes_.insert(notes_.begin(), std::move(n));
    while (notes_.size() > 500) {
      auto& oldest = notes_.back();
      dir = dir_;
      std::string file = dir.empty() ? std::string() : note_file(dir, oldest.id);
      notes_.pop_back();
      if (!file.empty()) remove_file(file);
    }
    dir = dir_;
  }
  if (!dir.empty()) {
    QuickNote added{id, text, 0};
    {
      std::lock_guard<std::mutex> lock(mu_);
      for (auto& n : notes_)
        if (n.id == id) {
          added = n;
          break;
        }
    }
    write_note_file(dir, added);
  }
  return id;
}

bool QuickNoteStore::remove(const std::string& id_or_index) {
  std::string gone;
  {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto it = notes_.begin(); it != notes_.end(); ++it) {
      if (it->id == id_or_index) {
        gone = it->id;
        notes_.erase(it);
        break;
      }
    }
    if (gone.empty()) {
      try {
        int idx = std::stoi(id_or_index);
        if (idx >= 1 && idx <= static_cast<int>(notes_.size())) {
          gone = notes_.begin()[static_cast<std::size_t>(idx - 1)].id;
          notes_.erase(notes_.begin() + (idx - 1));
        }
      } catch (...) {
      }
    }
    if (!gone.empty() && !dir_.empty()) remove_file(note_file(dir_, gone));
  }
  return !gone.empty();
}

void QuickNoteStore::clear() {
  std::string dir;
  {
    std::lock_guard<std::mutex> lock(mu_);
    dir = dir_;
    notes_.clear();
  }
  if (dir.empty()) return;
  // Only our own footer-marked files; anything else in the dir is the
  // user's and stays (it still surfaces through notes search).
  std::error_code ec;
  for (const auto& de : fs::directory_iterator(fs::u8path(dir), ec)) {
    if (ec) break;
    std::error_code ec2;
    if (!de.is_regular_file(ec2)) continue;
    auto p = de.path();
    auto ext = p.extension().u8string();
    std::string exts(ext.begin(), ext.end());
    if (to_lower_utf8(exts) != ".md") continue;
    auto full_u8 = p.u8string();
    std::string blob;
    if (!read_file_all(std::string(full_u8.begin(), full_u8.end()), blob)) continue;
    auto stem_u8 = p.stem().u8string();
    QuickNote n;
    if (parse_note_file(std::string(stem_u8.begin(), stem_u8.end()), blob, n))
      remove_file(std::string(full_u8.begin(), full_u8.end()));
  }
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
