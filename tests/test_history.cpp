#include "test.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/core/mmap.hpp"

#include <filesystem>

void test_history() {
  using namespace wilfred;
  HistoryStore h;
  h.set_enabled(true);
  h.set_max(3);
  h.record_query("firefox");
  h.record_query("minecraft");
  h.record_query("firefox");
  CHECK_EQ(h.recent_queries().front(), "firefox");
  CHECK(h.recent_queries().size() <= 3);

  h.record_selection("/apps/Code");
  h.record_selection("/apps/Code");
  CHECK_EQ(h.frequency("/apps/Code"), 2);
  CHECK(h.last_selected("/apps/Code") != 0);

  CHECK_EQ(h.query_frequency("firefox"), 2);
  h.record_choice("code", "/apps/Code");
  h.record_choice("code", "/apps/Code");
  CHECK_EQ(h.choice_count("code", "/apps/Code"), 2);
  auto sug = h.suggest_queries("fir", 4);
  CHECK(!sug.empty());
  CHECK_EQ(sug.front(), "firefox");

  auto p = (std::filesystem::temp_directory_path() / "wilfred_hist.dat").string();
  CHECK(h.save(p));
  HistoryStore h2;
  CHECK(h2.load(p));
  CHECK_EQ(h2.frequency("/apps/Code"), 2);
  CHECK(!h2.recent_queries().empty());
  CHECK_EQ(h2.query_frequency("firefox"), 2);
  CHECK_EQ(h2.choice_count("code", "/apps/Code"), 2);

  h2.clear();
  CHECK_EQ(h2.frequency("/apps/Code"), 0);
  CHECK(h2.recent_queries().empty());

  HistoryStore off;
  off.set_enabled(false);
  off.record_query("x");
  off.record_selection("/x");
  CHECK(off.recent_queries().empty());
  CHECK_EQ(off.frequency("/x"), 0);
}
