#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/search/clip_history.hpp"
#include "wilfred/search/define.hpp"
#include "wilfred/search/file_ops.hpp"
#include "wilfred/search/fuzzy.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/nettools.hpp"
#include "wilfred/search/pins.hpp"
#include "wilfred/sources/sources.hpp"
#include "wilfred/sync/backup.hpp"
#include "wilfred/ui/web_ui.hpp"

#include <filesystem>

void test_new_batch() {
  using namespace wilfred;
  namespace fs = std::filesystem;

  // --- Config: ui accent/font_size, sync encrypt, pins, ranking.pinned ---
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text(
        "ui:\n  theme: dark\n  accent: \"#7f8cff\"\n  font_size: 16\n"
        "sync:\n  enabled: false\n  encrypt: true\n  password: s3cret\n"
        "pins:\n  - firefox\n  - /home/user/docs\n"
        "ranking:\n  pinned: 900\n",
        cfg, err));
    CHECK_EQ(cfg.ui.accent, "#7f8cff");
    CHECK_EQ(cfg.ui.font_size, 16);
    CHECK(cfg.sync.encrypt);
    CHECK_EQ(cfg.sync.password, "s3cret");
    CHECK_EQ(cfg.pins.size(), 2u);
    CHECK_EQ(cfg.ranking.pinned, 900);
    Config bad;
    CHECK(!load_config_text("sync:\n  encrypt: true\n", bad, err));
    CHECK(!load_config_text("ui:\n  font_size: 99\n", bad, err));
  }

  // --- Encrypted sync roundtrip ---
  {
    std::vector<BackupEntry> files = {{"wilfred.yml", "a: 1"}, {"notes/x.md", "# hi"}};
    std::string enc, err;
    CHECK(pack_backup_archive_encrypted(files, "pw123", enc, err));
    CHECK(enc.compare(0, 7, "WILFEK1") == 0);
    std::vector<BackupEntry> out;
    CHECK(unpack_backup_archive_auto(enc, "pw123", out, err));
    CHECK_EQ(out.size(), 2u);
    std::vector<BackupEntry> bad_out;
    CHECK(!unpack_backup_archive_auto(enc, "wrong", bad_out, err));
    CHECK(!unpack_backup_archive_auto(enc, "", bad_out, err));
    // Plain archives still unpack via auto.
    std::string plain;
    CHECK(pack_backup_archive(files, plain, err));
    std::vector<BackupEntry> pout;
    CHECK(unpack_backup_archive_auto(plain, "", pout, err));
    CHECK_EQ(pout.size(), 2u);
  }

  // --- Offline define/thesaurus ---
  {
    DefineHit hit;
    CHECK(define_lookup("resilient", hit));
    CHECK(!hit.definition.empty());
    CHECK(!hit.synonyms.empty());
    CHECK(define_lookup("  UBIQUITOUS. ", hit));
    CHECK(!define_lookup("qzxwv", hit));
    auto sug = define_suggest("res", 5);
    CHECK(!sug.empty());
    CHECK(define_suggest("", 5).empty());
  }

  // --- Pins store ---
  {
    auto& pins = PinStore::instance();
    pins.configure("", {});
    pins.clear();
    CHECK(pins.add("Firefox"));
    CHECK(!pins.add("firefox"));  // dup (lowercased)
    CHECK(pins.contains("FIREFOX"));
    CHECK(pins.matches("Mozilla Firefox", "/apps/firefox.exe"));
    CHECK(!pins.matches("Chrome", "/apps/chrome.exe"));
    CHECK(pins.remove("firefox"));
    CHECK_EQ(pins.size(), 0u);
    pins.clear();
  }

  // --- Clipboard kinds persist ---
  {
    ClipStore& cs = ClipStore::instance();
    auto tmp = (fs::temp_directory_path() / "wilf-clips-test.bin").string();
    cs.configure(50, true, tmp);
    cs.clear_all();
    cs.record("hello world");
    cs.record_path("/tmp/photo.png");
    cs.record_image("image 800x600 png");
    auto entries = cs.entries();
    bool saw_path = false, saw_image = false;
    for (auto& e : entries) {
      if (e.kind == ClipKind::Path && e.text == "/tmp/photo.png") saw_path = true;
      if (e.kind == ClipKind::Image) saw_image = true;
    }
    CHECK(saw_path);
    CHECK(saw_image);
    CHECK(cs.save_now());
    CHECK(cs.load());
    cs.clear_all();
    std::error_code ec;
    fs::remove(tmp, ec);
  }

  // --- Event/contact creation ---
  {
    auto base = (fs::temp_directory_path() / "wilf-sources-test").string();
    Config cfg;
    cfg.sources.calendar_paths = {base + "/cal"};
    cfg.sources.contacts_paths = {base + "/contacts"};
    std::string dest, err;
    CHECK(create_calendar_event(cfg, "Team sync", "tomorrow 10am", dest, err));
    CHECK(!dest.empty());
    CHECK(create_contact(cfg, "Jane Doe", "jane@x.com", "555-0100", dest, err));
    CHECK(!dest.empty());
    CHECK(!create_calendar_event(cfg, "", "", dest, err));
    CHECK(!create_contact(cfg, "", "", "", dest, err));
    std::error_code ec;
    fs::remove_all(base, ec);
  }

  // --- Bulk file ops ---
  {
    auto dir = (fs::temp_directory_path() / "wilf-fileops-test").string();
    std::error_code ec;
    fs::remove_all(dir, ec);
    fs::create_directories(dir, ec);
    for (auto n : {"a.txt", "b.txt"}) {
      FILE* f = fopen((dir + "/" + n).c_str(), "wb");
      if (f) {
        fwrite("x", 1, 1, f);
        fclose(f);
      }
    }
    std::vector<std::string> renamed;
    std::string err;
    CHECK(bulk_rename_in_dir(dir, "photo-{n}.txt", renamed, err));
    CHECK_EQ(renamed.size(), 2u);
    std::vector<std::string> moved;
    auto dest = dir + "-dest";
    fs::remove_all(dest, ec);
    CHECK(move_paths_to({renamed.front()}, dest, moved, err));
    CHECK_EQ(moved.size(), 1u);
    std::string created;
    CHECK(create_from_template(dest, "python", "script", created, err));
    CHECK(!created.empty());
    CHECK(!create_from_template(dest, "nope", "x", created, err));
    fs::remove_all(dir, ec);
    fs::remove_all(dest, ec);
  }

  // --- Mini intents for the new queries ---
  {
    CHECK(parse_mini_intent("define resilient").kind == MiniKind::Define);
    CHECK(parse_mini_intent("thesaurus happy").kind == MiniKind::Define);
    CHECK(parse_mini_intent("pins").kind == MiniKind::Pins);
    CHECK(parse_mini_intent("pin firefox").kind == MiniKind::PinOp);
    CHECK(parse_mini_intent("unpin firefox").kind == MiniKind::PinOp);
    CHECK(parse_mini_intent("event add Team sync | tomorrow").kind == MiniKind::EventAdd);
    CHECK(parse_mini_intent("contact add Jane j@x.com").kind == MiniKind::ContactAdd);
    CHECK(parse_mini_intent("rename ./d photo-{n}.jpg").kind == MiniKind::FileOp);
    CHECK(parse_mini_intent("template python foo").kind == MiniKind::FileOp);
    Config cfg;
    auto cards = mini_results("define resilient", cfg, "");
    CHECK(!cards.empty());
    auto pins = mini_results("pins", cfg, "");
    CHECK(!pins.empty());
  }

  // --- Overlay appearance JSON carries accent/fontSize ---
  {
    Config cfg;
    cfg.ui.accent = "#ff5500";
    cfg.ui.font_size = 18;
    set_overlay_appearance(cfg);
    auto js = overlay_show_json();
    CHECK(js.find("ff5500") != std::string::npos);
    CHECK(js.find("18") != std::string::npos);
    Config def;
    set_overlay_appearance(def);
    CHECK_EQ(overlay_show_json(), "{\"type\":\"show\"}");
  }
}
