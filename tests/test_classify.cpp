#include "test.hpp"
#include "wilfred/query/classify.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/math/expr.hpp"
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
  CHECK_EQ(classify_query("windows").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("switch chrome").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("emoji").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("symbol arrow").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("fx").kind, QueryKind::Mini);

  set_currency_network_enabled(false);
  CHECK(looks_like_math("100 usd to eur"));
  CHECK_EQ(classify_query("100 usd to eur").kind, QueryKind::Math);
  CHECK_EQ(classify_query("10 dollars in euros").kind, QueryKind::Math);
  set_currency_network_enabled(true);
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

  set_currency_network_enabled(false);
  auto ccy = interp.interpret("100 usd to eur", cfg, nullptr);
  CHECK_EQ(ccy.classification.kind, QueryKind::Math);
  CHECK(!ccy.results.empty());
  CHECK_EQ(ccy.results.front().action, ResultAction::Convert);
  CHECK(ccy.results.front().subtitle.find("Currency") != std::string::npos);
  auto em = interp.interpret("emoji fire", cfg, nullptr);
  CHECK_EQ(em.classification.kind, QueryKind::Mini);
  CHECK(!em.results.empty());
  CHECK_EQ(em.results.front().action, ResultAction::Copy);
  set_currency_network_enabled(true);
}
