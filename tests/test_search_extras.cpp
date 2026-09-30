#include "test.hpp"

#include "wilfred/core/paths.hpp"
#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/index/store.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/math/expr.hpp"
#include "wilfred/platform/native.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/content.hpp"
#include "wilfred/search/context.hpp"
#include "wilfred/search/macros.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/rank.hpp"

void test_search_extras() {
  using namespace wilfred;

  set_mini_network_enabled(false);
  set_currency_network_enabled(false);

  auto w = parse_mini_intent("weather");
  CHECK_EQ(w.kind, MiniKind::Weather);
  CHECK(w.remainder.empty());
  auto w2 = parse_mini_intent("weather London");
  CHECK_EQ(w2.kind, MiniKind::Weather);
  CHECK_EQ(w2.remainder, "London");
  CHECK_EQ(parse_mini_intent("time").kind, MiniKind::Time);
  CHECK_EQ(parse_mini_intent("disk").kind, MiniKind::Disk);
  CHECK_EQ(parse_mini_intent("disku").kind, MiniKind::DiskUsage);
  CHECK_EQ(parse_mini_intent("ram").kind, MiniKind::Ram);
  CHECK_EQ(parse_mini_intent("process chrome").kind, MiniKind::Process);
  CHECK_EQ(parse_mini_intent("ps").kind, MiniKind::Process);
  CHECK_EQ(parse_mini_intent("help").kind, MiniKind::Help);
  CHECK_EQ(parse_mini_intent("speedtest").kind, MiniKind::Speedtest);
  CHECK(parse_mini_intent("speedtest").exact);
  CHECK_EQ(parse_mini_intent("speed-test").kind, MiniKind::Speedtest);
  CHECK_EQ(parse_mini_intent("speedtest again").remainder, "again");
  CHECK(parse_mini_intent("speedtest again").exact);
  CHECK_EQ(parse_mini_intent("speedtest foo").kind, MiniKind::None);
  CHECK_EQ(parse_mini_intent("netspeed").kind, MiniKind::Speedtest);
  CHECK_EQ(parse_mini_intent("bandwidth").kind, MiniKind::Speedtest);
  CHECK_EQ(parse_mini_intent("internetspeed").kind, MiniKind::Speedtest);
  CHECK_EQ(parse_mini_intent("macros").kind, MiniKind::MacrosList);
  CHECK_EQ(parse_mini_intent("windows").kind, MiniKind::Windows);
  CHECK_EQ(parse_mini_intent("window chrome").kind, MiniKind::Windows);
  CHECK_EQ(parse_mini_intent("window chrome").remainder, "chrome");
  CHECK_EQ(parse_mini_intent("switch").kind, MiniKind::Windows);
  CHECK_EQ(parse_mini_intent("firefox").kind, MiniKind::None);
  CHECK_EQ(parse_mini_intent("emoji").kind, MiniKind::Emoji);
  CHECK_EQ(parse_mini_intent("emoji fire").kind, MiniKind::Emoji);
  CHECK_EQ(parse_mini_intent("emoji fire").remainder, "fire");
  CHECK_EQ(parse_mini_intent("emojis").kind, MiniKind::Emoji);
  CHECK_EQ(parse_mini_intent("symbol").kind, MiniKind::Symbol);
  CHECK_EQ(parse_mini_intent("symbols arrow").remainder, "arrow");
  CHECK_EQ(parse_mini_intent("fx").kind, MiniKind::Fx);
  CHECK_EQ(parse_mini_intent("currency 100 usd to eur").kind, MiniKind::Fx);
  CHECK_EQ(parse_mini_intent("fx 100 usd eur").remainder, "100 usd eur");
  CHECK_EQ(parse_mini_intent("tz").kind, MiniKind::Tz);
  CHECK_EQ(parse_mini_intent("tz tokyo").remainder, "tokyo");
  CHECK_EQ(parse_mini_intent("color").kind, MiniKind::Color);
  CHECK_EQ(parse_mini_intent("colour #fff").remainder, "#fff");
  CHECK_EQ(parse_mini_intent("uuid").kind, MiniKind::Uuid);
  CHECK_EQ(parse_mini_intent("base64 hello").kind, MiniKind::Base64);
  CHECK_EQ(parse_mini_intent("sha256 abc").kind, MiniKind::Sha256);
  CHECK_EQ(parse_mini_intent("lorem 12").kind, MiniKind::Lorem);
  CHECK_EQ(parse_mini_intent("json {\"a\":1}").kind, MiniKind::Json);
  CHECK_EQ(parse_mini_intent("lock").kind, MiniKind::System);
  CHECK_EQ(parse_mini_intent("lock").remainder, "lock");
  CHECK_EQ(parse_mini_intent("shutdown").kind, MiniKind::System);
  CHECK_EQ(parse_mini_intent("empty trash").remainder, "empty_trash");
  CHECK_EQ(parse_mini_intent("sleep").remainder, "sleep");
  CHECK_EQ(parse_mini_intent("restart now").remainder, "restart");
  CHECK_EQ(parse_mini_intent("log out").remainder, "logout");
  CHECK_EQ(parse_mini_intent("lock firefox").kind, MiniKind::None);
  CHECK_EQ(parse_mini_intent("hash").kind, MiniKind::None);
  CHECK_EQ(parse_mini_intent("hash abc").kind, MiniKind::Sha256);

  Config cfg;
  auto time_cards = mini_results("time", cfg, "");
  CHECK(!time_cards.empty());
  CHECK_EQ(time_cards.front().category, "mini");
  auto ram_cards = mini_results("ram", cfg, "");
  CHECK(!ram_cards.empty());
  auto help_cards = mini_results("help", cfg, "");
  CHECK(help_cards.size() >= 4);
  auto wins = mini_results("windows", cfg, "");
  CHECK(!wins.empty());
  CHECK(wins.front().category == "window" || wins.front().kind_label == "window");
  (void)native_list_windows();

  auto emoji_cards = mini_results("emoji smile", cfg, "");
  CHECK(!emoji_cards.empty());
  CHECK_EQ(emoji_cards.front().kind_label, "emoji");
  CHECK_EQ(emoji_cards.front().action, ResultAction::Copy);
  CHECK(!emoji_cards.front().payload.empty());
  auto smile = mini_results("emoji", cfg, "");
  CHECK(smile.size() >= 8);
  auto sym_cards = mini_results("symbol euro", cfg, "");
  CHECK(!sym_cards.empty());
  CHECK_EQ(sym_cards.front().kind_label, "symbol");
  auto fx_cards = mini_results("fx 100 usd to eur", cfg, "");
  CHECK(!fx_cards.empty());
  CHECK(fx_cards.front().action == ResultAction::Convert);
  auto fx_short = mini_results("fx 25 gbp jpy", cfg, "");
  CHECK(!fx_short.empty());
  CHECK_EQ(fx_short.front().kind_label, "fx");
  auto fx_board = mini_results("fx", cfg, "");
  CHECK(fx_board.size() >= 4);
  auto tz_cards = mini_results("tz tokyo", cfg, "");
  CHECK(!tz_cards.empty());
  CHECK_EQ(tz_cards.front().kind_label, "tz");
  auto color_cards = mini_results("color #00ff00", cfg, "");
  CHECK(color_cards.size() >= 3);
  CHECK_EQ(color_cards.front().kind_label, "color");
  auto time_zone = mini_results("time london", cfg, "");
  CHECK(time_zone.size() >= 2);

  CHECK(looks_like_math("uuid"));
  CHECK(looks_like_math("sha256 abc"));
  auto uuid_cards = mini_results("uuid", cfg, "");
  CHECK(!uuid_cards.empty());
  CHECK_EQ(uuid_cards.front().kind_label, "uuid");
  auto b64_cards = mini_results("base64 hi", cfg, "");
  CHECK(!b64_cards.empty());
  CHECK_EQ(b64_cards.front().payload, "aGk=");
  auto lock_cards = mini_results("lock", cfg, "");
  CHECK(!lock_cards.empty());
  CHECK_EQ(lock_cards.front().action, ResultAction::System);
  CHECK_EQ(lock_cards.front().payload, "lock");
  CHECK_EQ(lock_cards.front().kind_label, "lock");
  auto json_cards = mini_results("json {\"a\":1}", cfg, "");
  CHECK(!json_cards.empty());
  CHECK(json_cards.front().payload.find("\"a\"") != std::string::npos);
  auto trash_cards = mini_results("empty trash", cfg, "");
  CHECK(!trash_cards.empty());
  CHECK_EQ(trash_cards.front().payload, "empty_trash");
  CHECK(!native_system_action("not-a-system-action"));

  Config cfg_off;
  cfg_off.search.minis = false;
  CHECK(mini_results("time", cfg_off, "").empty());

  ClipboardSnapshot snap;
  snap.text = path_join(home_directory(), "Documents/notes.md") + "\nsecret token WidgetFactory";
  set_clipboard_override(snap);
  CHECK(clipboard_text_matches("widgetfactory", snap.text));
  CHECK(!clipboard_text_matches("zzzz", snap.text));
  auto hints = clipboard_path_hints(snap);
  CHECK(!hints.empty());
  CHECK(!clipboard_preview(snap.text, 20).empty());
  CHECK(!clipboard_history_texts().empty());

  auto url = expand_macro("https://www.youtube.com/results?search_query={query}", "cute cats", "");
  CHECK(url.find("cute") != std::string::npos);
  auto clip_url = expand_macro("https://example.com/?q={clipboard_enc}", "", "hello world");
  CHECK(clip_url.find("hello") != std::string::npos);

  auto m = match_macro("!yt cats", cfg);
  CHECK(m.matched);
  CHECK_EQ(m.name, "yt");
  CHECK_EQ(m.argument, "cats");
  auto cards = macro_results(m, "");
  CHECK(!cards.empty());
  CHECK_EQ(cards.front().action, ResultAction::WebSearch);

  auto bare = match_macro("yt", cfg);
  CHECK(bare.matched);
  auto weak = match_macro("g", cfg);
  CHECK(!weak.matched);

  CHECK(content_indexable(FileKind::Source, "foo.cpp"));
  CHECK(looks_like_text_extension(".md"));
  auto toks = extract_content_tokens("alpha beta the WidgetFactory", 32);
  CHECK(!toks.empty());

  IndexStore store;
  IndexRecord rec;
  rec.kind = FileKind::Source;
  auto id = store.upsert(rec, "/tmp/notes.cpp");
  store.add_content_tokens(id, {"widgetfactory", "alpha"});
  CHECK(store.has_content_tokens(id));
  CHECK_EQ(store.content_token_hits(id, {"widgetfactory"}), 1);
  CHECK(store.content_covers_tokens(id, {"widgetfactory"}));

  RankContext ctx;
  fill_rank_context(ctx, "widgetfactory", cfg, nullptr, snap.text);
  ctx.allow_content = true;
  ctx.context_aware = true;
  auto* r = store.get(id);
  CHECK(r);
  int sc = rank_record(ctx, store, *r, cfg.ranking);
  CHECK(sc > 0);

  HistoryStore hist;
  hist.set_enabled(true);
  hist.record_query("projects");
  hist.record_selection("/work/Projects/alpha.cpp");
  RankContext ctx2;
  fill_rank_context(ctx2, "alpha", cfg, &hist, "");
  CHECK(ctx2.context_aware);
  CHECK(!ctx2.recent_parents.empty());

  IndexEngine index;
  SearchEngine search(index);
  QueryInterpreter interp(index, search);
  auto iq = interp.interpret("time", cfg, nullptr);
  CHECK_EQ(iq.classification.kind, QueryKind::Mini);
  CHECK(!iq.results.empty());

  auto st = interp.interpret("speedtest", cfg, nullptr);
  CHECK_EQ(st.classification.kind, QueryKind::Mini);
  CHECK(!st.results.empty());
  CHECK_EQ(st.results.front().kind_label, "speedtest");
  auto st2 = interp.interpret("speedtest again", cfg, nullptr);
  CHECK_EQ(st2.classification.kind, QueryKind::Mini);
  CHECK(!st2.results.empty());
  auto st3 = interp.interpret("speedtest again", cfg, nullptr);
  CHECK_EQ(st3.results.front().kind_label, "speedtest");

  auto iq2 = interp.interpret("!yt cats", cfg, nullptr);
  CHECK(!iq2.results.empty());
  bool has_macro = false;
  for (auto& s : iq2.results)
    if (s.category == "macro") has_macro = true;
  CHECK(has_macro);

  auto iq3 = interp.interpret("clip", cfg, nullptr);
  CHECK(!iq3.results.empty());

  set_clipboard_override(std::nullopt);
  set_mini_network_enabled(true);
  set_currency_network_enabled(true);
}
