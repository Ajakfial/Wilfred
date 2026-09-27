#include "test.hpp"
#include "wilfred/query/classify.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/search/engine.hpp"

void test_classify() {
  using namespace wilfred;
  CHECK_EQ(classify_query("").kind, QueryKind::Empty);
  CHECK(looks_like_math("25 * 42"));
  CHECK(!looks_like_math("42"));
  CHECK(!looks_like_math("minecraft"));
  CHECK(looks_like_url("https://example.com"));
  CHECK(looks_like_url("www.example.com"));
  CHECK(!looks_like_url("C:\\Windows\\System32"));
  CHECK_EQ(classify_query("https://example.com").kind, QueryKind::Url);
  CHECK_EQ(classify_query("? cats").kind, QueryKind::WebSearch);
  CHECK_EQ(classify_query("g cats").kind, QueryKind::WebSearch);
  CHECK_EQ(classify_query("25 * 42").kind, QueryKind::Math);
  CHECK_EQ(classify_query("*.cpp in Projects").kind, QueryKind::FilteredSearch);
  CHECK_EQ(classify_query("> notepad").kind, QueryKind::Command);
  CHECK_EQ(classify_query("weather").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("!yt cats").kind, QueryKind::Macro);
  CHECK_EQ(classify_query("yt cats").kind, QueryKind::Macro);

  Config cfg;
  IndexEngine index;
  SearchEngine search(index);
  QueryInterpreter interp(index, search);
  auto math = interp.interpret("2+2", cfg, nullptr);
  CHECK_EQ(math.classification.kind, QueryKind::Math);
  CHECK(!math.results.empty());
  CHECK_EQ(math.results.front().action, ResultAction::Calculate);

  auto url = interp.interpret("https://example.com", cfg, nullptr);
  CHECK_EQ(url.classification.kind, QueryKind::Url);
  CHECK(!url.results.empty());

  auto web = interp.interpret("? cats", cfg, nullptr);
  CHECK_EQ(web.classification.kind, QueryKind::WebSearch);
  CHECK_EQ(web.results.front().action, ResultAction::WebSearch);
  auto conv = interp.interpret("10 km to mi", cfg, nullptr);
  CHECK_EQ(conv.classification.kind, QueryKind::Math);
  CHECK(!conv.results.empty());
  CHECK_EQ(conv.results.front().action, ResultAction::Convert);
}
