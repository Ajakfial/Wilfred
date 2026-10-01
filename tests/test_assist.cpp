#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/search/fuzzy.hpp"
#include "wilfred/search/suggest.hpp"
#include "wilfred/ui/web_ui.hpp"

void test_assist() {
  using namespace wilfred;

  // Damerau: transposition is distance 1 (unlike plain Levenshtein).
  CHECK_EQ(damerau_bounded("fireofx", "firefox", 2), 1);
  CHECK_EQ(damerau_bounded("weahter", "weather", 2), 1);
  CHECK_EQ(typo_threshold(2), 0);
  CHECK_EQ(typo_threshold(4), 1);
  CHECK_EQ(typo_threshold(6), 2);
  CHECK(is_typo_match("firefoz", "firefox"));
  CHECK(is_typo_match("weah", "weat"));
  CHECK(!is_typo_match("xyz", "abc"));
  CHECK(is_typo_prefix("firefo", "firefox"));
  CHECK(is_typo_prefix("weah", "weather"));

  // score_fuzzy is typo-aware (single substitution / transposition).
  {
    auto a = score_fuzzy("firefoz", "firefox", "firefox");
    CHECK(a.matched);
    auto b = score_fuzzy("fireofx", "firefox", "firefox");
    CHECK(b.matched);
    auto c = score_fuzzy("visaul", "visual studio code", "Visual Studio Code");
    CHECK(c.matched);
  }

  // Command correction.
  {
    Config cfg;
    CHECK_EQ(correct_command_key("weahter", cfg), "weather");
    CHECK_EQ(correct_command_key("timm", cfg), "time");
    CHECK(correct_command_key("weather", cfg).empty());
    CHECK_EQ(correct_query_command("weahter london", cfg), "weather london");
    HistoryStore h;
    CHECK_EQ(suggest_correction("weahter", cfg, &h), "weather");
    CHECK(suggest_correction("weather", cfg, &h).empty());
    auto cands = autocomplete_candidates("wea", cfg, &h, 4);
    CHECK(!cands.empty());
    CHECK_EQ(cands.front(), "weather");
    auto ghost = autocomplete_ghost("wea", cfg, &h);
    CHECK_EQ(ghost, "weather");
  }

  // History-aware ghost + correction.
  {
    Config cfg;
    HistoryStore h;
    h.record_query("firefox");
    h.record_query("firefox");
    auto cands = autocomplete_candidates("fire", cfg, &h, 4);
    CHECK(!cands.empty());
    CHECK_EQ(cands.front(), "firefox");
    CHECK_EQ(suggest_correction("firefoz", cfg, &h), "firefox");
  }

  // Assist JSON shape (cross-platform contract).
  {
    SearchResult r;
    r.title = "Firefox";
    r.path = "/apps/firefox";
    r.kind = FileKind::Application;
    std::vector<SearchResult> items{r};
    auto json = overlay_results_json(items, {}, "firefox", "firefox", {"firefox"}, "firefoz");
    CHECK(json.find("\"correction\":\"firefox\"") != std::string::npos);
    CHECK(json.find("\"ghost\":\"firefox\"") != std::string::npos);
    CHECK(json.find("\"candidates\":[\"firefox\"]") != std::string::npos);
    CHECK(json.find("\"query\":\"firefoz\"") != std::string::npos);
    // Legacy 2-arg overload still works (empty assist).
    auto legacy = overlay_results_json(items);
    CHECK(legacy.find("\"correction\":\"\"") != std::string::npos);
  }
}
