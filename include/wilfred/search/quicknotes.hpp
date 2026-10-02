#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace wilfred {

struct QuickNote {
  std::string id;
  std::string text;
  std::int64_t when{0};
};

struct TodoItem {
  int id{0};
  std::string text;
  bool done{false};
  std::int64_t when{0};
};

// Quick notes live as real Markdown files (`<id>.md`) inside the configured
// notes directory, so they are readable, editable, and searchable outside
// Wilfred. Todos stay in `<data dir>/todos.txt` (tasks are not documents).
class QuickNoteStore {
 public:
  static QuickNoteStore& instance();
  // Configure the notes directory (e.g. `<data dir>/notes`). Empty means
  // memory-only, which is what unit tests use. A legacy `quicknotes.txt`
  // next to the directory is imported once on load().
  void configure(std::string dir);
  bool load();
  bool save_now();
  std::string add(const std::string& text);
  bool remove(const std::string& id_or_index);
  void clear();
  std::vector<QuickNote> list(const std::string& query = {}, int limit = 12) const;
  std::size_t size() const;

 private:
  QuickNoteStore() = default;
  mutable std::mutex mu_;
  std::vector<QuickNote> notes_;
  std::string dir_;
  int next_{1};
};

class TodoStore {
 public:
  static TodoStore& instance();
  void configure(std::string path);
  bool load();
  bool save_now();
  std::string add(const std::string& text);
  bool set_done(int id, bool done);
  bool remove(int id);
  void clear_done();
  void clear_all();
  std::vector<TodoItem> list(bool include_done = true, const std::string& query = {},
                             int limit = 16) const;
  std::size_t size() const;
  std::size_t open_count() const;

 private:
  TodoStore() = default;
  mutable std::mutex mu_;
  std::vector<TodoItem> items_;
  std::string path_;
  int next_{1};
};

}  // namespace wilfred
