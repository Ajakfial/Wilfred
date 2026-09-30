#include "test.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/math/expr.hpp"
#include "wilfred/query/classify.hpp"
#include "wilfred/query/interpreter.hpp"
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
  CHECK_EQ(classify_query("speedtest").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("speedtest again").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("bandwidth").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("windows").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("switch chrome").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("emoji").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("symbol arrow").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("fx").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("uuid").kind, QueryKind::Math);
  CHECK_EQ(classify_query("sha256 abc").kind, QueryKind::Math);
  CHECK_EQ(classify_query("json {\"a\":1}").kind, QueryKind::Math);
  CHECK_EQ(classify_query("lorem 8").kind, QueryKind::Math);
  CHECK_EQ(classify_query("lock").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("shutdown").kind, QueryKind::Mini);
  CHECK_EQ(classify_query("empty trash").kind, QueryKind::Mini);

  set_currency_network_enabled(false);
  CHECK(looks_like_math("100 usd to eur"));
  CHECK_EQ(classify_query("100 usd to eur").kind, QueryKind::Math);
  CHECK_EQ(classify_query("10 dollars in euros").kind, QueryKind::Math);
  CHECK(looks_like_math("#ff5500"));
  CHECK_EQ(classify_query("#00ff00").kind, QueryKind::Math);
  CHECK(looks_like_math("rgb(1, 2, 3)"));
  CHECK(looks_like_math("2024-01-01 + 10 days"));
  CHECK(looks_like_math("3pm est to pst"));
  CHECK(looks_like_math("now in tokyo"));
  CHECK_EQ(classify_query("now in london").kind, QueryKind::Math);
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
  auto col = interp.interpret("#ff0000", cfg, nullptr);
  CHECK_EQ(col.classification.kind, QueryKind::Math);
  CHECK(col.results.size() >= 3);
  CHECK_EQ(col.results.front().action, ResultAction::Convert);
  auto dt = interp.interpret("2024-06-15 + 1 day", cfg, nullptr);
  CHECK_EQ(dt.classification.kind, QueryKind::Math);
  CHECK(dt.results.front().subtitle.find("Date/time") != std::string::npos);
  auto uid = interp.interpret("uuid", cfg, nullptr);
  CHECK_EQ(uid.classification.kind, QueryKind::Math);
  CHECK(!uid.results.empty());
  CHECK_EQ(uid.results.front().action, ResultAction::Convert);
  CHECK_EQ(uid.results.front().kind_label, "uuid");
  CHECK(uid.results.front().subtitle.find("Dev") != std::string::npos);
  auto js = interp.interpret("json {\"a\":1}", cfg, nullptr);
  CHECK_EQ(js.classification.kind, QueryKind::Math);
  CHECK_EQ(js.results.front().kind_label, "json");
  auto lk = interp.interpret("lock", cfg, nullptr);
  CHECK_EQ(lk.classification.kind, QueryKind::Mini);
  CHECK_EQ(lk.results.front().action, ResultAction::System);
  CHECK_EQ(lk.results.front().kind_label, "lock");
  auto em = interp.interpret("emoji fire", cfg, nullptr);
  CHECK_EQ(em.classification.kind, QueryKind::Mini);
  CHECK(!em.results.empty());
  CHECK_EQ(em.results.front().action, ResultAction::Copy);
  set_currency_network_enabled(true);
}
