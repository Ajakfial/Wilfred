#include "test.hpp"

#include "wilfred/browser/library.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/core/paths.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/clip_history.hpp"
#include "wilfred/search/clipboard.hpp"
#include "wilfred/search/doctext.hpp"
#include "wilfred/search/file_ops.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/semantic.hpp"
#include "wilfred/ui/web_ui.hpp"

#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include "wilfred/fs/walker.hpp"
#endif

namespace fs = std::filesystem;

namespace {

fs::path power_root() {
  auto root = fs::temp_directory_path() / "wilfred_test_power";
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);
  return root;
}

void write_bytes(const fs::path& p, const std::string& data) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary);
  f.write(data.data(), static_cast<std::streamsize>(data.size()));
}

}  // namespace

void test_power() {
  using namespace wilfred;
  auto root = power_root();

  // ---- Clipboard manager store ----
  {
    auto& store = ClipStore::instance();
    store.configure(10, false, "");
    store.clear_all();
    store.record("alpha one");
    store.record("beta two");
    store.record("alpha one");  // dedupe to front
    auto texts = store.texts();
    CHECK(texts.size() == 2);
    CHECK(texts.front() == "alpha one");
    CHECK(store.pin_text("beta two"));
    CHECK(store.pinned("beta two"));
    texts = store.texts();
    CHECK(texts.front() == "beta two");  // pinned floats first
    auto hits = store.search("alp", 8);
    CHECK(hits.size() == 1);
    CHECK(store.unpin_text("beta two"));
    store.clear_unpinned();
    CHECK(store.size() == 0);

    // Persist / reload round trip.
    auto clipfile = (root / "clips.bin").string();
    store.configure(10, true, clipfile);
    store.record("keep me");
    store.record("temp");
    CHECK(store.pin_text("keep me"));
    CHECK(store.save_now());
    store.clear_all();
    CHECK(store.size() == 0);
    CHECK(store.load());
    CHECK(store.pinned("keep me"));
    CHECK(store.texts().size() == 2);
    store.configure(200, true, "");
    store.clear_all();
  }

  // ---- Document text extraction ----
  {
    CHECK(is_document_extension("a.docx"));
    CHECK(is_document_extension("a.PDF"));
    CHECK(!is_document_extension("a.txt"));
    CHECK(!is_document_extension("a.exe"));

    std::string out;
    CHECK(!extract_document_text((root / "nope.docx").string(), out, 65536));

    // Minimal .docx (stored entries are valid zip).
    auto docsrc = root / "docsrc" / "word";
    write_bytes(docsrc / "document.xml",
                "<?xml version=\"1.0\"?><w:document><w:body><w:p><w:r><w:t>Hello "
                "Docx World</w:t></w:r></w:p></w:body></w:document>");
    auto docx = (root / "hello.docx").string();
    {
      std::string err;
      CHECK(zip_paths_to({(root / "docsrc" / "word").string()}, docx, err));
    }
    CHECK(extract_document_text(docx, out, 65536));
    CHECK(out.find("Hello") != std::string::npos);
    CHECK(out.find("Docx") != std::string::npos);

    // Minimal .odt.
    auto odtsrc = root / "odtsrc";
    write_bytes(odtsrc / "content.xml",
                "<?xml version=\"1.0\"?><office:document-content><office:body><office:text><text:p>"
                "Odt Sample Text</text:p></office:text></office:body></office:document-content>");
    auto odt = (root / "sample.odt").string();
    {
      std::string err;
      CHECK(zip_paths_to({odtsrc.string()}, odt, err));
    }
    CHECK(extract_document_text(odt, out, 65536));
    CHECK(out.find("Odt") != std::string::npos);

    // Uncompressed PDF stream.
    auto pdf = root / "hello.pdf";
    write_bytes(pdf,
                "%PDF-1.4\n1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
                "4 0 obj\n<< /Length 60 >>\nstream\nBT /F1 12 Tf 72 720 Td (Hello "
                "Wilfred World) Tj ET\nendstream\nendobj\n");
    CHECK(extract_document_text(pdf.string(), out, 65536));
    CHECK(out.find("Wilfred") != std::string::npos);

    // RTF + HTML.
    auto rtf = root / "hello.rtf";
    write_bytes(rtf,
                "{\\rtf1\\ansi{\\fonttbl{\\f0\\fswiss Helvetica;}}\\f0\\par Hello "
                "{\\b bold} world\\par}");
    CHECK(extract_document_text(rtf.string(), out, 65536));
    CHECK(out.find("Hello") != std::string::npos);
    CHECK(out.find("fonttbl") == std::string::npos);
    auto html = root / "hello.html";
    write_bytes(html,
                "<html><head><script>var secret=1;</script></head><body><p>Hello "
                "<b>Html</b> World</p></body></html>");
    CHECK(extract_document_text(html.string(), out, 65536));
    CHECK(out.find("Html") != std::string::npos);
    CHECK(out.find("secret") == std::string::npos);
  }

  // ---- Semantic (trigram) scoring ----
  {
    CHECK_NEAR(trigram_cosine("hello world", "hello world"), 1.0, 1e-9);
    CHECK(trigram_cosine("abc", "xyz") == 0);
    double close = trigram_cosine("photo editor", "photo editor pro");
    double far = trigram_cosine("photo editor", "zebra xylophone");
    CHECK(close > 0.6);
    CHECK(far < 0.2);
    CHECK(close > far);

    IndexEngine index;
    IndexRecord r1;
    r1.kind = FileKind::Application;
    index.store().upsert(r1, "/apps/Visual Studio Code");
    IndexRecord r2;
    r2.kind = FileKind::Application;
    index.store().upsert(r2, "/apps/Firefox");
    IndexRecord r3;
    r3.kind = FileKind::Document;
    index.store().upsert(r3, "/docs/Annual Report 2024.pdf");
    SemanticProvider prov(index);
    Config cfg;
    cfg.providers.semantic = true;
    cfg.providers.semantic_min_score = 0.2;
    auto hits = prov.query("visual studio code", cfg, 10);
    CHECK(!hits.empty());
    CHECK(hits.front().title.find("Visual Studio") != std::string::npos);
    auto typo = prov.query("anual report", cfg, 10);
    CHECK(!typo.empty());
    CHECK(typo.front().title.find("Annual") != std::string::npos);
    Config off;
    CHECK(prov.query("visual studio code", off, 10).empty());
  }

  // ---- Browser bookmarks parser ----
  {
    auto prof = root / "chromeprof";
    write_bytes(prof / "Bookmarks",
                "{\"roots\":{\"bookmark_bar\":{\"children\":[{\"name\":\"Wilfred "
                "Repo\",\"type\":\"url\",\"url\":\"https://example.com/wilfred\"},{\"name\":\"Fol"
                "der\",\"type\":\"folder\",\"children\":[{\"name\":\"Nested "
                "Link\",\"type\":\"url\",\"url\":\"https://example.com/nested\"}]}],\"name\":\"Book"
                "marks bar\"},\"other\":{\"children\":[],\"name\":\"Other\"}}}");
    std::vector<LibraryItem> items;
    CHECK(parse_chromium_bookmarks_file((prof / "Bookmarks").string(), items, 100));
    CHECK(items.size() == 2);
    CHECK(items[0].url == "https://example.com/wilfred");
    CHECK(items[1].title == "Nested Link");
    CHECK(!parse_chromium_bookmarks_file((root / "missing").string(), items, 100));

    CHECK_EQ(parse_mini_intent("bm wilfred").kind, MiniKind::Browser);
    CHECK_EQ(parse_mini_intent("bookmarks").kind, MiniKind::Browser);
    CHECK_EQ(parse_mini_intent("tabs").kind, MiniKind::Browser);
    CHECK_EQ(parse_mini_intent("history chrome").kind, MiniKind::Browser);
    Config cfg;
    auto cards = mini_results("bm nothing-matches-this-xyz", cfg, "");
    CHECK(!cards.empty());
  }

  // ---- Workflow chaining ----
  {
    set_clipboard_override(ClipboardSnapshot{});
    SearchResult r;
    r.path = "C:\\work\\notes.txt";
    r.payload = "payload-text";
    r.kind = FileKind::File;
    CHECK(execute_result_action(r, Config{}, "copy_path+copy_text"));
    CHECK(read_clipboard().text == "payload-text");
    // A failing step makes the whole chain fail (hash of a missing file
    // fails on every platform), but earlier steps still ran.
    SearchResult missing;
    missing.path = (root / "does-not-exist.txt").string();
    missing.kind = FileKind::File;
    CHECK(!execute_result_action(missing, Config{}, "hash_file"));
    CHECK(!execute_result_action(missing, Config{}, "copy_path+hash_file"));
    CHECK(read_clipboard().text == missing.path);
    SearchResult ow;
    ow.path = "C:\\work\\notes.txt";
    ow.kind = FileKind::File;
    // open_with targets containing '+' must not be split into a chain.
    CHECK(!execute_result_action(ow, Config{}, "open_with:C:\\x+y\\missing-app-xyz.exe"));
    set_clipboard_override(std::nullopt);
  }

  // ---- Preview builder ----
  {
    auto txt = root / "prev.txt";
    write_bytes(txt, "line one\nline two\nline three\n");
    auto pv = build_file_preview(txt.string());
    CHECK(pv.error.empty());
    CHECK(pv.text.find("line one") != std::string::npos);
    CHECK(pv.title == "prev.txt");
    auto missing = build_file_preview((root / "nope.txt").string());
    CHECK(!missing.error.empty());
    auto dirpv = build_file_preview(root.string());
    CHECK(dirpv.text.find("items") != std::string::npos);
    auto png = root / "tiny.png";
    std::string fake("\x89PNG\r\n\x1a\n", 8);
    fake.append(64, '\0');
    write_bytes(png, fake);
    auto imgpv = build_file_preview(png.string());
    CHECK(imgpv.image_data_url.rfind("data:image/png;base64,", 0) == 0);
    auto js = overlay_preview_json(txt.string());
    CHECK(js.find("\"type\":\"preview\"") != std::string::npos);
    CHECK(js.find("line two") != std::string::npos);
  }

#ifdef _WIN32
  // ---- USN parent-chain resolver (pure) ----
  {
    std::unordered_map<std::uint64_t, UsnNode> nodes;
    nodes[5] = UsnNode{5, L"C:", true};
    nodes[10] = UsnNode{5, L"Users", true};
    nodes[11] = UsnNode{10, L"a.txt", false};
    nodes[99] = UsnNode{100, L"orphan.txt", false};
    auto paths = usn_resolve_paths(nodes, "C:\\");
    CHECK(paths[11] == "C:\\Users\\a.txt");
    CHECK(paths[99] == "C:\\orphan.txt");
  }
#endif

  // ---- New config keys ----
  {
    Config c;
    ConfigError e;
    CHECK(
        load_config_text("clipboard:\n  max_entries: 50\nproviders:\n  semantic: true\n  "
                         "semantic_min_score: 0.5\nbrowser:\n  library: false\nindex:\n  "
                         "usn_scan: false\n",
                         c, e));
    CHECK_EQ(c.clipboard.max_entries, 50);
    CHECK(c.providers.semantic);
    CHECK_NEAR(c.providers.semantic_min_score, 0.5, 1e-9);
    CHECK(!c.browser.library);
    CHECK(!c.index.usn_scan);
  }

  std::error_code ec;
  fs::remove_all(root, ec);
}
