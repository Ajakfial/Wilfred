#include "test.hpp"
#include "wilfred/search/fuzzy.hpp"
#include "wilfred/index/tokenizer.hpp"

void test_fuzzy() {
  using namespace wilfred;
  CHECK(is_subsequence("vsc", "visualstudiocode"));
  CHECK(is_subsequence("code", "visual studio code"));
  CHECK(!is_subsequence("xyz", "abc"));

  CHECK(acronym_match("vsc", "Visual Studio Code"));
  CHECK(acronym_match("vscode", "Visual Studio Code") ||
        score_fuzzy("vscode", fold_search("visual studio code"), "Visual Studio Code").matched);

  auto vscode = score_fuzzy("vscode", fold_search("visual studio code"), "Visual Studio Code");
  CHECK(vscode.matched);
  auto exact = score_fuzzy("discord", "discord", "Discord");
  CHECK(exact.matched);
  CHECK(exact.score > vscode.score);

  auto disc = score_fuzzy("disc", "discord", "Discord");
  CHECK(disc.matched);
  CHECK(disc.score >= 700);

  auto sys = score_fuzzy("sys32", fold_search("system32"), "System32");
  CHECK(sys.matched);

  auto proj = score_fuzzy("myproj", fold_search("myproject"), "MyProject");
  CHECK(proj.matched);

  CHECK_EQ(levenshtein_bounded("kitten", "kitten", 2), 0);
  CHECK_EQ(levenshtein_bounded("kitten", "sitten", 2), 1);
  CHECK(levenshtein_bounded("abcdef", "abzzzz", 2) > 2);

  auto miss = score_fuzzy("zzzznotfound", "readme", "README");
  CHECK(!miss.matched || miss.score < 100);
}
