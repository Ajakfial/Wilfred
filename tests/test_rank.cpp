#include "test.hpp"
#include "wilfred/search/rank.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/core/time_util.hpp"

void test_rank() {
  using namespace wilfred;
  IndexStore store;
  IndexRecord app;
  app.kind = FileKind::Application;
  app.flags = RecordFlags::Application;
  auto app_id = store.upsert(app, "/Applications/Visual Studio Code.app");

  IndexRecord file;
  file.kind = FileKind::Source;
  auto file_id = store.upsert(file, "/tmp/code.cpp");

  RankContext ctx;
  ctx.query = "code";
  ctx.folded = fold_search("code");
  ctx.tokens = tokenize_name("code");
  RankingWeights w;

  auto* appr = store.get(app_id);
  auto* filer = store.get(file_id);
  CHECK(appr && filer);
  int sa = rank_record(ctx, store, *appr, w);
  int sf = rank_record(ctx, store, *filer, w);
  CHECK(sa > 0);
  CHECK(sf > 0);

  ctx.frequency["/Applications/Visual Studio Code.app"] = 40;
  ctx.last_selected["/Applications/Visual Studio Code.app"] = unix_seconds();
  int sa2 = rank_record(ctx, store, *appr, w);
  CHECK(sa2 > sa);

  ctx.aliases["code"] = "Visual Studio Code";
  int sa3 = rank_record(ctx, store, *appr, w);
  CHECK(sa3 >= sa2);

  RankContext learned = ctx;
  learned.learned_paths["/Applications/Visual Studio Code.app"] = 3;
  int saLearn = rank_record(learned, store, *appr, w);
  CHECK(saLearn > sa3);

  default_rank_pipeline().clear();
  default_rank_pipeline().add("boost_apps", [](const RankContext&, const IndexStore&,
                                               const IndexRecord& rec, const RankingWeights&) {
    return rec.kind == FileKind::Application ? 50 : 0;
  });
  int sa4 = rank_record(ctx, store, *appr, w);
  CHECK(sa4 > sa3);
  default_rank_pipeline().clear();

  std::vector<ScoredHit> hits{{file_id, sf, filer}, {app_id, sa3, appr}};
  auto top = take_top(std::move(hits), 1);
  CHECK_EQ(top.size(), 1u);
  CHECK_EQ(top[0].id, app_id);
}
