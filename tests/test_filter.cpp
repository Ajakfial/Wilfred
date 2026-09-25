#include "test.hpp"
#include "wilfred/search/filter.hpp"
#include "wilfred/index/tokenizer.hpp"

void test_filter() {
  using namespace wilfred;
  std::string q = "*.cpp in Projects large pdf files modified recently";
  auto f = parse_filter_clauses(q);
  CHECK(!f.extensions.empty());
  bool has_cpp = false, has_pdf = false;
  for (auto& e : f.extensions) {
    auto l = fold_search(e);
    if (l == ".cpp" || l == "cpp") has_cpp = true;
    if (l == ".pdf" || l == "pdf") has_pdf = true;
  }
  CHECK(has_cpp);
  CHECK(has_pdf);
  CHECK(!f.in_dirs.empty());
  CHECK(f.min_size.has_value());
  CHECK(f.min_mtime.has_value());

  std::string q2 = "type:image ext:png name:logo hidden:true";
  auto f2 = parse_filter_clauses(q2);
  CHECK(!f2.kinds.empty());
  CHECK(!f2.extensions.empty());
  CHECK(!f2.name_contains.empty());
  CHECK(f2.hidden.has_value() && *f2.hidden);

  std::string q3 = "applications containing browser";
  auto f3 = parse_filter_clauses(q3);
  CHECK(f3.apps_only.has_value() || !f3.kinds.empty());
  CHECK(!f3.name_contains.empty());

  bool ok = false;
  CHECK_EQ(parse_size_token("10mb", ok), 10ull * 1024 * 1024);
  CHECK(ok);
  parse_time_token("2d", ok);
  CHECK(ok);

  IndexStore store;
  IndexRecord rec;
  rec.kind = FileKind::Source;
  rec.size = 100;
  rec.mtime = 1;
  auto id = store.upsert(rec, "/work/Projects/foo.cpp");
  auto* r = store.get(id);
  CHECK(r);
  SearchFilter only_cpp;
  only_cpp.extensions.push_back(".cpp");
  CHECK(record_matches_filter(only_cpp, store, *r));
  only_cpp.extensions = {".pdf"};
  CHECK(!record_matches_filter(only_cpp, store, *r));
  SearchFilter in_proj;
  in_proj.in_dirs.push_back("Projects");
  CHECK(record_matches_filter(in_proj, store, *r));

  Config cfg;
  cfg.scopes["home"].push_back("/work");
  SearchFilter scoped;
  scoped.scope_names.push_back("home");
  apply_named_scopes(scoped, cfg);
  CHECK_EQ(scoped.in_dirs.size(), 1u);
}
