#include "test.hpp"
#include "wilfred/index/tokenizer.hpp"

void test_tokenizer() {
  auto t = wilfred::tokenize_name("Visual Studio Code");
  CHECK_EQ(t.size(), 3u);
  CHECK_EQ(t[0], "visual");
  CHECK_EQ(wilfred::acronym_of("Visual Studio Code"), "vsc");
  CHECK_EQ(wilfred::fold_search("AbC"), "abc");
  auto tri = wilfred::trigrams("cat");
  CHECK(!tri.empty());
  CHECK(wilfred::glob_match("file.tmp", "*.tmp"));
  CHECK(wilfred::glob_match("MyFile.CPP", "*.cpp"));
  CHECK(!wilfred::glob_match("file.cpp", "*.tmp"));
  auto camel = wilfred::tokenize_name("MyProject");
  CHECK_EQ(camel.size(), 2u);
}
