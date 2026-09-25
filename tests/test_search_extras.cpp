#include "test.hpp"

#include "wilfred/history/history.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/index/store.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/content.hpp"
#include "wilfred/search/context.hpp"
#include "wilfred/search/macros.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/rank.hpp"
#include "wilfred/index/tokenizer.hpp"

void test_search_extras() {
  using namespace wilfred;

  set_mini_network_enabled(false);

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
  CHECK_EQ(parse_mini_intent("macros").kind, MiniKind::MacrosList);
  CHECK_EQ(parse_mini_intent("firefox").kind, MiniKind::None);

  Config cfg;
  auto time_cards = mini_results("time", cfg, "");
  CHECK(!time_cards.empty());
  CHECK_EQ(time_cards.front().category, "mini");
  auto ram_cards = mini_results("ram", cfg, "");
  CHECK(!ram_cards.empty());
  auto help_cards = mini_results("help", cfg, "");
  CHECK(help_cards.size() >= 4);

  Config cfg_off;
  cfg_off.search.minis = false;
  CHECK(mini_results("time", cfg_off, "").empty());

  ClipboardSnapshot snap;
  snap.text = "C:\\Users\\jayla\\Documents\\notes.md\nsecret token WidgetFactory";
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
}
