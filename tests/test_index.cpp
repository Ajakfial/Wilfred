#include "test.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/index/store.hpp"
#include "wilfred/index/wal.hpp"
#include "wilfred/search/engine.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static fs::path make_temp_tree() {
  auto root = fs::temp_directory_path() / "wilfred_test_index";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root / "Projects" / "MyProject", ec);
  fs::create_directories(root / "docs", ec);
  {
    std::ofstream(root / "Projects" / "MyProject" / "main.cpp") << "int main(){}\n";
    std::ofstream(root / "docs" / "notes.txt") << "hello\n";
    std::ofstream(root / "Projects" / "file with spaces.pdf") << "x";
    std::ofstream(root / "docs" / "unicode_test.txt") << "u";
    std::ofstream(root / "hidden.tmp") << "tmp";
    fs::create_directories(root / "文档", ec);
    std::ofstream(root / "文档" / "café file.txt") << "unicode\n";
    std::ofstream(root / "docs" / "weird#name$.cpp") << "int x;\n";
  }
  return root;
}

void test_index() {
  using namespace wilfred;
  IndexStore store;
  IndexRecord rec;
  rec.kind = FileKind::Source;
  rec.size = 42;
  auto id = store.upsert(rec, "/tmp/foo.cpp");
  CHECK(id != 0);
  CHECK(store.get(id) != nullptr);
  CHECK(store.by_path("/tmp/foo.cpp") != nullptr);
  CHECK_EQ(store.live_count(), 1u);

  IndexRecord rec2;
  rec2.kind = FileKind::Source;
  rec2.size = 99;
  auto id2 = store.upsert(rec2, "/tmp/foo.cpp");
  CHECK_EQ(id, id2);
  CHECK_EQ(store.by_path("/tmp/foo.cpp")->size, 99u);

  CHECK(store.rename_path("/tmp/foo.cpp", "/tmp/bar.cpp"));
  CHECK(store.by_path("/tmp/foo.cpp") == nullptr);
  CHECK(store.by_path("/tmp/bar.cpp") != nullptr);

  CHECK(store.remove_path("/tmp/bar.cpp"));
  CHECK(store.by_path("/tmp/bar.cpp") == nullptr);
  CHECK_EQ(store.live_count(), 0u);

  auto snap = (fs::temp_directory_path() / "wilfred_store.wilf").string();
  IndexRecord rec3;
  rec3.kind = FileKind::Directory;
  rec3.flags = RecordFlags::Directory;
  store.upsert(rec3, "/tmp/dir with spaces");
  CHECK(store.save(snap));
  IndexStore loaded;
  CHECK(loaded.load(snap));
  CHECK(loaded.by_path("/tmp/dir with spaces") != nullptr);

  {
    std::ofstream bad(snap, std::ios::binary | std::ios::app);
    bad.put(char(0xFF));
  }
  IndexStore corrupt;
  CHECK(!corrupt.load(snap));

  auto tree = make_temp_tree();
  auto idxdir = (fs::temp_directory_path() / "wilfred_idx_engine").string();
  fs::remove_all(idxdir);
  fs::create_directories(idxdir);

  Config cfg;
  cfg.index.paths = {tree.string()};
  cfg.index.ext_exclude = {".tmp"};
  cfg.index.workers = 2;
  cfg.search.min_query_length = 1;

  {
    IndexEngine eng;
    CHECK(eng.open(idxdir, cfg));
    CHECK(eng.upsert_file((tree / "Projects" / "MyProject" / "main.cpp").string()));
    CHECK(eng.upsert_file((tree / "docs" / "notes.txt").string()));
    CHECK(eng.upsert_file((tree / "Projects" / "file with spaces.pdf").string()));
    CHECK(!eng.upsert_file((tree / "nope" / "missing.txt").string()) ||
          eng.store().by_path((tree / "nope" / "missing.txt").string()) == nullptr);
    auto from = (tree / "docs" / "notes.txt").string();
    auto to = (tree / "docs" / "renamed.txt").string();
    fs::rename(from, to);
    CHECK(eng.rename_path(from, to));
    CHECK(eng.store().by_path(to) != nullptr);
    eng.scan_roots();
    eng.checkpoint();
    CHECK(eng.store().live_count() > 0);

    SearchEngine se(eng);
    auto hits = se.search("main", cfg, nullptr, 20);
    CHECK(!hits.empty());
    bool found_main = false;
    for (auto& h : hits)
      if (h.title.find("main") != std::string::npos) found_main = true;
    CHECK(found_main);

    auto cafe = se.search("cafe", cfg, nullptr, 20);
    auto weird = se.search("weird", cfg, nullptr, 20);
    CHECK(!weird.empty());

    auto filtered = se.search("*.cpp in Projects", cfg, nullptr, 40);
    CHECK(!filtered.empty());

    auto proj = se.search("myproj", cfg, nullptr, 20);
    CHECK(!proj.empty());

    auto cached = se.search("main", cfg, nullptr, 20);
    CHECK_EQ(cached.size(), hits.size());

    for (int n = 0; n < 2500; ++n) {
      IndexRecord r;
      r.kind = FileKind::File;
      eng.store().upsert(r, "/synthetic/item_" + std::to_string(n) + ".dat");
    }
    CHECK(eng.store().live_count() >= 2500);
  }

  {
    IndexEngine eng2;
    CHECK(eng2.open(idxdir, cfg));
    CHECK(eng2.store().live_count() > 0);
  }

  auto walp = (fs::temp_directory_path() / "wilfred_test.wal").string();
  fs::remove(walp);
  {
    WriteAheadLog wal;
    CHECK(wal.open(walp));
    IndexRecord r;
    r.kind = FileKind::File;
    CHECK(wal.append_upsert(r, "/tmp/wal.txt"));
    CHECK(wal.append_delete("/tmp/wal.txt"));
    CHECK(wal.append_rename("/a", "/b"));
    CHECK(wal.append_checkpoint());
    wal.close();
  }
  {
    WriteAheadLog wal;
    CHECK(wal.open(walp));
    std::vector<WalEntry> entries;
    CHECK(wal.replay(entries));
    CHECK(entries.size() >= 3);
  }

  StringPool pool;
  for (int i = 0; i < 20000; ++i)
    pool.intern("token_" + std::to_string(i));
  CHECK_EQ(pool.find("token_123"), pool.intern("token_123"));
  CHECK(pool.find("missing_token") == StringPool::kInvalid);

#ifdef _WIN32
  auto link = tree / "link_to_notes";
  std::error_code lec;
  fs::create_symlink(tree / "docs" / "renamed.txt", link, lec);
  if (!lec) {
    IndexEngine eng;
    eng.open(idxdir, cfg);
    eng.upsert_file(link.string());
  }
#endif

  // ---- Snapshot format v2 vs v3 ----
  {
    IndexStore s;
    const char* exts[] = {".cpp", ".hpp", ".md", ".txt", ".png"};
    for (int i = 0; i < 300; ++i) {
      IndexRecord r;
      r.kind = (i % 7 == 0) ? FileKind::Directory : FileKind::Source;
      r.size = static_cast<std::uint64_t>(i * 13);
      r.mtime = 1700000000 + i;
      s.upsert(r, "/data/proj/module" + std::to_string(i % 17) + "/file" + std::to_string(i) +
                      exts[i % 5]);
    }
    CHECK(s.remove_path("/data/proj/module3/file20.cpp"));
    CHECK(s.rename_path("/data/proj/module1/file18.txt", "/data/proj/renamed ARTICLE.md"));
    auto v2snap = (fs::temp_directory_path() / "wilfred_store_v2.wilf").string();
    auto v3snap = (fs::temp_directory_path() / "wilfred_store_v3.wilf").string();
    s.set_write_version(2);
    CHECK_EQ(s.write_version(), 2);
    CHECK(s.save(v2snap));
    s.set_write_version(3);
    CHECK_EQ(s.write_version(), 3);
    CHECK(s.save(v3snap));
    s.set_write_version(99);  // clamps to latest
    CHECK_EQ(s.write_version(), 3);
    auto version_of = [](const std::string& p) {
      std::string blob;
      if (!read_file_all(p, blob) || blob.size() < 8) return 0u;
      return static_cast<unsigned>(static_cast<std::uint8_t>(blob[4])) |
             (static_cast<unsigned>(static_cast<std::uint8_t>(blob[5])) << 8) |
             (static_cast<unsigned>(static_cast<std::uint8_t>(blob[6])) << 16) |
             (static_cast<unsigned>(static_cast<std::uint8_t>(blob[7])) << 24);
    };
    CHECK_EQ(version_of(v2snap), 2u);
    CHECK_EQ(version_of(v3snap), 3u);

    IndexStore a, b;
    CHECK(a.load(v2snap));
    CHECK(b.load(v3snap));
    CHECK_EQ(a.live_count(), b.live_count());
    CHECK_EQ(a.live_count(), s.live_count());
    // Records identical.
    for (auto& r : s.records()) {
      if (r.id == 0) continue;
      auto* ar = a.get(r.id);
      auto* br = b.get(r.id);
      if (!s.get(r.id)) {
        CHECK(ar == nullptr && br == nullptr);
        continue;
      }
      CHECK(ar != nullptr && br != nullptr);
      CHECK_EQ(ar->size, r.size);
      CHECK_EQ(br->mtime, r.mtime);
      CHECK_EQ((int)br->kind, (int)r.kind);
    }
    // Secondary indexes identical (postings adopted, not rebuilt).
    CHECK_EQ(a.prefix_index().size(), b.prefix_index().size());
    for (std::size_t i = 0; i < a.prefix_index().size(); ++i)
      CHECK_EQ(a.prefix_index()[i], b.prefix_index()[i]);
    for (auto tok : {"proj", "data", "renamed", "article", "md", "png", "cpp"}) {
      auto ta = a.pool().find(tok);
      auto tb = b.pool().find(tok);
      CHECK(ta != StringPool::kInvalid && tb != StringPool::kInvalid);
      const auto& pa = a.posting(ta);
      const auto& pb = b.posting(tb);
      CHECK_EQ(pa.size(), pb.size());
      for (std::size_t i = 0; i < pa.size(); ++i)
        CHECK_EQ(pa[i], pb[i]);
    }
    {
      auto ta = a.pool().find("ile");
      auto tb = b.pool().find("ile");
      if (ta != StringPool::kInvalid && tb != StringPool::kInvalid)
        CHECK_EQ(a.trigram(ta).size(), b.trigram(tb).size());
    }
    CHECK_EQ(a.ext_index().size(), b.ext_index().size());
    CHECK(b.by_path("/data/proj/renamed ARTICLE.md") != nullptr);
    CHECK(b.by_path("/data/proj/module3/file20.cpp") == nullptr);
    // Incremental updates keep working on adopted postings.
    IndexRecord extra;
    extra.kind = FileKind::Document;
    b.upsert(extra, "/data/proj/new/notes EXTRA.md");
    CHECK_EQ(b.posting(b.pool().find("extra")).size(), 1u);
    auto v3b = (fs::temp_directory_path() / "wilfred_store_v3b.wilf").string();
    CHECK(b.save(v3b));
    IndexStore c;
    CHECK(c.load(v3b));
    CHECK(c.by_path("/data/proj/new/notes EXTRA.md") != nullptr);
    CHECK_EQ(c.posting(c.pool().find("extra")).size(), 1u);
  }
}
