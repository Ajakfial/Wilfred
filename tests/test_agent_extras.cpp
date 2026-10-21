// Tests for the fork additions: saved searches, overlay keybindings, the
// recent/saved/meta minis, and the hardened ZIP extractor.

#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/crc32.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/search/disktools.hpp"
#include "wilfred/search/file_ops.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/updater/updater.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

void put16(std::vector<std::uint8_t>& b, std::uint16_t v) {
  b.push_back(static_cast<std::uint8_t>(v & 0xFF));
  b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void put32(std::vector<std::uint8_t>& b, std::uint32_t v) {
  for (int i = 0; i < 4; ++i) b.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
}

// Single-entry, stored (uncompressed) ZIP. Enough to exercise the extractor
// and its traversal guard without pulling in a third-party writer.
std::vector<std::uint8_t> make_stored_zip(const std::string& name, const std::string& body) {
  const auto crc = wilfred::crc32(body.data(), body.size());
  const auto n = static_cast<std::uint16_t>(name.size());
  const auto sz = static_cast<std::uint32_t>(body.size());
  std::vector<std::uint8_t> z;

  // Local file header.
  put32(z, 0x04034b50);
  put16(z, 20);
  put16(z, 0);
  put16(z, 0);  // method = store
  put16(z, 0);
  put16(z, 0);
  put32(z, crc);
  put32(z, sz);
  put32(z, sz);
  put16(z, n);
  put16(z, 0);
  z.insert(z.end(), name.begin(), name.end());
  z.insert(z.end(), body.begin(), body.end());

  const auto cd_offset = static_cast<std::uint32_t>(z.size());
  // Central directory.
  put32(z, 0x02014b50);
  put16(z, 20);
  put16(z, 20);
  put16(z, 0);
  put16(z, 0);
  put16(z, 0);
  put16(z, 0);
  put32(z, crc);
  put32(z, sz);
  put32(z, sz);
  put16(z, n);
  put16(z, 0);
  put16(z, 0);
  put16(z, 0);
  put16(z, 0);
  put32(z, 0);
  put32(z, 0);
  z.insert(z.end(), name.begin(), name.end());
  const auto cd_size = static_cast<std::uint32_t>(z.size() - cd_offset);

  // End of central directory.
  put32(z, 0x06054b50);
  put16(z, 0);
  put16(z, 0);
  put16(z, 1);
  put16(z, 1);
  put32(z, cd_size);
  put32(z, cd_offset);
  put16(z, 0);
  return z;
}

bool path_exists(const fs::path& p) {
  std::error_code ec;
  return fs::exists(p, ec);
}

}  // namespace

void test_agent_extras() {
  using namespace wilfred;

  // ---- searches: / keybinds: config ----------------------------------
  {
    wilfred::Config cfg;
    wilfred::ConfigError err;
    CHECK(load_config_text(
        "searches:\n"
        "  Inbox: \"type:email in Mail\"\n"
        "  big: \"size:>100mb\"\n"
        "keybinds:\n"
        "  preview: F4\n"
        "  actions: \"ctrl+shift+p\"\n",
        cfg, err));
    CHECK_EQ(cfg.searches.size(), 2u);
    CHECK_EQ(cfg.searches["inbox"], "type:email in Mail");
    CHECK_EQ(cfg.searches["big"], "size:>100mb");
    CHECK_EQ(cfg.keybinds["preview"], "F4");
    CHECK_EQ(cfg.keybinds["actions"], "ctrl+shift+p");

    // Unknown action, empty chord and wrong shapes all fail loudly.
    wilfred::Config bad;
    CHECK(!load_config_text("keybinds:\n  nope: F4\n", bad, err));
    CHECK(!err.message.empty());
    CHECK(!load_config_text("keybinds:\n  preview: \"\"\n", bad, err));
    CHECK(!load_config_text("keybinds: [a, b]\n", bad, err));
    CHECK(!load_config_text("searches: [a]\n", bad, err));
    CHECK(!load_config_text("searches:\n  empty: \"\"\n", bad, err));

    // Defaults are exposed for the overlay merge and cover the documented set.
    bool has_preview = false, has_actions = false;
    for (auto& [name, chord] : keybind_defaults()) {
      CHECK(!name.empty());
      CHECK(!chord.empty());
      if (name == "preview") has_preview = true;
      if (name == "actions") has_actions = true;
    }
    CHECK(has_preview);
    CHECK(has_actions);
  }

  // ---- mini routing ---------------------------------------------------
  {
    CHECK_EQ(parse_mini_intent("recent").kind, wilfred::MiniKind::Recent);
    CHECK_EQ(parse_mini_intent("recent 5").kind, wilfred::MiniKind::Recent);
    CHECK_EQ(parse_mini_intent("saved").kind, wilfred::MiniKind::Saved);
    CHECK_EQ(parse_mini_intent("searches").kind, wilfred::MiniKind::Saved);
    CHECK_EQ(parse_mini_intent("meta").kind, wilfred::MiniKind::Meta);
    CHECK_EQ(parse_mini_intent("metadata").kind, wilfred::MiniKind::Meta);
    CHECK_EQ(parse_mini_intent("recentx").kind, wilfred::MiniKind::None);
  }

  // ---- saved / meta cards ---------------------------------------------
  {
    wilfred::Config cfg;
    auto empty_saved = mini_results("saved", cfg, "");
    CHECK_EQ(empty_saved.size(), 1u);
    CHECK_EQ(empty_saved.front().action, wilfred::ResultAction::None);

    cfg.searches["inbox"] = "type:email in Mail";
    cfg.searches["big"] = "size:>100mb";
    auto saved = mini_results("saved", cfg, "");
    CHECK_EQ(saved.size(), 2u);
    // Sorted by name, and Enter inserts the query (Habit), not opens a file.
    CHECK_EQ(saved.front().title, "big");
    CHECK_EQ(saved.front().action, wilfred::ResultAction::Habit);
    CHECK_EQ(saved.front().payload, "size:>100mb ");

    auto one = mini_results("saved inbox", cfg, "");
    CHECK_EQ(one.size(), 1u);
    CHECK_EQ(one.front().title, "inbox");

    wilfred::Config meta_cfg;
    meta_cfg.custom_metadata["owner"] = "jay";
    meta_cfg.custom_metadata["machine"] = "studio";
    auto meta = mini_results("meta", meta_cfg, "");
    CHECK_EQ(meta.size(), 2u);
    CHECK_EQ(meta.front().title, "machine");
    CHECK_EQ(meta.front().payload, "studio");
    CHECK_EQ(meta.front().action, wilfred::ResultAction::Copy);

    auto owner = mini_results("meta OWNER", meta_cfg, "");
    CHECK_EQ(owner.size(), 1u);
    CHECK_EQ(owner.front().payload, "jay");

    auto missing = mini_results("meta nope", meta_cfg, "");
    CHECK_EQ(missing.size(), 1u);
    CHECK_EQ(missing.front().action, wilfred::ResultAction::None);
  }

  // ---- recent: needs an index, then ranks by mtime ---------------------
  {
    wilfred::Config cfg;
    auto no_index = mini_results("recent", cfg, "");
    CHECK_EQ(no_index.size(), 1u);
    CHECK_EQ(no_index.front().action, wilfred::ResultAction::None);

    auto root = fs::temp_directory_path() / "wilfred_agent_extras";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "files", ec);
    fs::create_directories(root / "idx", ec);
    CHECK(wilfred::write_file_atomic((root / "files" / "old.txt").string(), "old", 3));
    CHECK(wilfred::write_file_atomic((root / "files" / "new.txt").string(), "new", 3));
    // Give the two files distinct mtimes so the ordering is deterministic.
    fs::last_write_time(root / "files" / "old.txt", fs::file_time_type::clock::now() - std::chrono::hours(48), ec);
    fs::last_write_time(root / "files" / "new.txt", fs::file_time_type::clock::now() - std::chrono::hours(1), ec);

    wilfred::IndexEngine eng;
    wilfred::Config icfg;
    icfg.index.paths = {(root / "files").string()};
    CHECK(eng.open((root / "idx").string(), icfg));
    CHECK(eng.upsert_file((root / "files" / "old.txt").string()));
    CHECK(eng.upsert_file((root / "files" / "new.txt").string()));

    auto recent = wilfred::recent_file_results(eng, icfg, "", 10, 0);
    CHECK(!recent.empty());
    CHECK(recent.front().path.find("new.txt") != std::string::npos);

    // A one-hour window excludes the 48h-old file.
    const auto now = std::chrono::duration_cast<std::chrono::seconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
    auto hour = wilfred::recent_file_results(eng, icfg, "", 10, now - 3600);
    CHECK_EQ(hour.size(), 1u);
    CHECK(hour.front().path.find("new.txt") != std::string::npos);

    // Filter that matches nothing yields the empty-state card.
    auto none = wilfred::recent_file_results(eng, icfg, "does-not-exist", 10, 0);
    CHECK_EQ(none.size(), 1u);
    CHECK_EQ(none.front().action, wilfred::ResultAction::None);

    fs::remove_all(root, ec);
  }

  // ---- zip extraction: round trip + traversal guard -------------------
  {
    auto root = fs::temp_directory_path() / "wilfred_agent_zip";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root / "src", ec);
    CHECK(wilfred::write_file_atomic((root / "src" / "hello.txt").string(), "hello", 5));

    const auto archive = (root / "hello.zip").string();
    std::string err;
    CHECK(wilfred::zip_paths_to({(root / "src" / "hello.txt").string()}, archive, err));

    std::size_t entries = 0;
    CHECK(wilfred::unzip_to(archive, (root / "out").string(), entries, err));
    CHECK_EQ(entries, 1u);
    CHECK(path_exists(root / "out" / "hello.txt"));

    // A hostile entry name must be skipped, not written outside the target.
    auto evil = make_stored_zip("../escape.txt", "pwned");
    CHECK(wilfred::write_file_atomic((root / "evil.zip").string(), evil.data(), evil.size()));
    std::size_t evil_entries = 0;
    CHECK(wilfred::unzip_to((root / "evil.zip").string(), (root / "safe").string(), evil_entries, err));
    CHECK_EQ(evil_entries, 0u);
    CHECK(!path_exists(root / "escape.txt"));
    CHECK(!path_exists(root / "safe" / "escape.txt"));

    fs::remove_all(root, ec);
  }
}
