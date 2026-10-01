#include "test.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/file_ops.hpp"
#include "wilfred/updater/updater.hpp"

#include <filesystem>

namespace fs = std::filesystem;

namespace {

bool has_action(const wilfred::SearchResult& r, const std::string& id) {
  for (auto& a : r.actions)
    if (a.id == id) return true;
  return false;
}

bool has_open_with(const wilfred::SearchResult& r) {
  for (auto& a : r.actions)
    if (a.id.rfind("open_with:", 0) == 0) return true;
  return false;
}

std::string unique_tmp_root() {
  static int n = 0;
  auto root = (fs::temp_directory_path() / ("wilfred_fileact_" + std::to_string(++n))).string();
  std::error_code ec;
  fs::create_directories(fs::u8path(root), ec);
  return root;
}

void write_text(const std::string& path, const std::string& text) {
  FILE* f = nullptr;
#ifdef _WIN32
  _wfopen_s(&f, fs::u8path(path).wstring().c_str(), L"wb");
#else
  f = std::fopen(path.c_str(), "wb");
#endif
  if (!f) return;
  std::fwrite(text.data(), 1, text.size(), f);
  std::fclose(f);
}

}  // namespace

void test_file_actions() {
  using namespace wilfred;

  // Path flavors (pure string transforms).
  CHECK(path_to_posix("C:\\Users\\a\\b.txt") == "C:/Users/a/b.txt");
  CHECK(path_to_posix("/a/b") == "/a/b");
  CHECK(path_to_file_uri("/a/b c.txt") == "file:///a/b%20c.txt");
  CHECK(path_to_file_uri("C:\\a b\\c.txt") == "file:///C:/a%20b/c.txt");
  CHECK(path_to_wsl("C:\\Users\\a") == "/mnt/c/Users/a");
  CHECK(path_to_wsl("D:/x") == "/mnt/d/x");
  CHECK(path_to_wsl("/usr/bin").empty());

  auto root = unique_tmp_root();

  // SHA-256 of a known file ("abc" vector).
  auto abc = root + "/abc.txt";
  write_text(abc, "abc");
  std::string hex, err;
  CHECK(sha256_file(abc, hex, err));
  CHECK_EQ(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(!sha256_file(root + "/missing.txt", hex, err));
  CHECK(!sha256_file(root, hex, err));

  // Stored-zip round trip through the real extractor.
  auto sub = root + "/proj";
  std::error_code ec;
  fs::create_directories(fs::u8path(sub + "/inner"), ec);
  write_text(sub + "/a.txt", "hello");
  write_text(sub + "/inner/b.txt", "world");
  auto zip = root + "/proj.zip";
  CHECK(zip_paths_to({sub}, zip, err));
  CHECK(fs::exists(fs::u8path(zip), ec));
  auto out_dir = root + "/unzipped";
  CHECK(extract_zip(zip, out_dir, &err));
  CHECK(fs::exists(fs::u8path(out_dir + "/proj/a.txt"), ec));
  CHECK(fs::exists(fs::u8path(out_dir + "/proj/inner/b.txt"), ec));
  CHECK(!zip_paths_to({root + "/missing.txt"}, root + "/nope.zip", err));
  CHECK(!zip_paths_to({}, root + "/nope.zip", err));

  // Unique sibling names.
  auto first = unique_sibling_path(root, "Report", ".zip");
  CHECK(wilfred::path_filename(first) == "Report.zip");
  write_text(first, "x");
  auto second = unique_sibling_path(root, "Report", ".zip");
  CHECK(second != first);
  CHECK(wilfred::path_filename(second) == "Report 2.zip");

  // New file / folder creation.
  std::string created, cerr;
  CHECK(create_new_file_here(root, created, cerr));
  CHECK(fs::exists(fs::u8path(created), ec));
  std::string created2;
  CHECK(create_new_file_here(root, created2, cerr));
  CHECK(created2 != created);
  std::string newdir;
  CHECK(create_new_folder_here(root, newdir, cerr));
  CHECK(fs::is_directory(fs::u8path(newdir), ec));

  // Attachment: files get the full set, dirs skip hash/open-with,
  // applications keep the legacy set.
  SearchResult file;
  file.path = abc;
  file.title = "abc.txt";
  file.kind = FileKind::File;
  attach_result_actions(file);
  for (auto want :
       {"open", "reveal", "copy_path", "copy_name", "copy_file_uri", "hash_file", "compress_zip",
        "open_terminal", "open_editor"})
    CHECK(has_action(file, want));
  (void)native_apps_for_file(abc, 4);  // must not crash; may be empty on CI

  SearchResult dir;
  dir.path = sub;
  dir.title = "proj";
  dir.kind = FileKind::Directory;
  attach_result_actions(dir);
  CHECK(has_action(dir, "new_file"));
  CHECK(has_action(dir, "new_folder"));
  CHECK(has_action(dir, "open_terminal"));
  CHECK(!has_action(dir, "hash_file"));

  SearchResult app;
  app.path = "/usr/bin/firefox";
  app.title = "Firefox";
  app.kind = FileKind::Application;
  attach_result_actions(app);
  CHECK(has_action(app, "open"));
  CHECK(!has_action(app, "hash_file"));
  CHECK(!has_action(app, "compress_zip"));
  CHECK(!has_action(app, "open_terminal"));

  // Execution through the clipboard override (no OS side effects).
  set_clipboard_override(ClipboardSnapshot{});
  SearchResult posix;
  posix.path = "C:\\Users\\a\\b.txt";
  posix.kind = FileKind::File;
  CHECK(execute_result_action(posix, Config{}, "copy_posix"));
  CHECK(read_clipboard().text == "C:/Users/a/b.txt");
  CHECK(execute_result_action(posix, Config{}, "copy_file_uri"));
  CHECK(read_clipboard().text == "file:///C:/Users/a/b.txt");

  SearchResult hash;
  hash.path = abc;
  hash.kind = FileKind::File;
  CHECK(execute_result_action(hash, Config{}, "hash_file"));
  CHECK(read_clipboard().text ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  // create/compress execution paths reveal in the file manager (OS side
  // effect), so they are covered above via the direct function calls.
  SearchResult mk;
  mk.path = sub;
  mk.kind = FileKind::Directory;
  attach_result_actions(mk);
  CHECK(has_action(mk, "new_file"));
  CHECK(has_action(mk, "new_folder"));
  CHECK(has_action(mk, "compress_zip"));

  CHECK(action_hides_overlay("open_terminal"));
  CHECK(action_hides_overlay("open_with:/x"));
  CHECK(!action_hides_overlay("copy_posix"));
  CHECK(!action_hides_overlay("hash_file"));

  set_clipboard_override(std::nullopt);
  fs::remove_all(fs::u8path(root), ec);
}
