#include "test.hpp"
#include "wilfred/core/paths.hpp"

void test_paths() {
  using namespace wilfred;
  auto joined = path_join("C:/Users", "jayla");
  CHECK(!joined.empty());
  CHECK_EQ(path_filename("/tmp/foo.cpp"), "foo.cpp");
  CHECK_EQ(path_filename("C:\\Windows\\System32"), "System32");
  CHECK_EQ(path_stem("/tmp/foo.cpp"), "foo");
  CHECK_EQ(path_extension("/tmp/foo.CPP"), ".cpp");
  CHECK_EQ(path_parent("/tmp/a/b"), "/tmp/a");
  CHECK(path_is_absolute("/usr/bin"));
#ifdef _WIN32
  CHECK(path_is_absolute("C:\\Windows"));
  CHECK(path_is_under("C:\\Windows\\System32\\cmd.exe", "C:\\Windows"));
  CHECK(path_equals("C:/Windows", "C:\\Windows"));
#else
  CHECK(path_is_under("/usr/bin/ls", "/usr"));
  CHECK(!path_is_under("/usr", "/usr/bin"));
#endif
  CHECK(!path_is_absolute("relative/path"));
  auto home = home_directory();
  CHECK(!home.empty());
  CHECK(!config_directory().empty());
  CHECK(!data_directory().empty());
  CHECK(!default_index_roots().empty());
  CHECK(!default_system_directories().empty());
  auto spaced = path_join("/tmp", "file with spaces.txt");
  CHECK_EQ(path_filename(spaced), "file with spaces.txt");
}
