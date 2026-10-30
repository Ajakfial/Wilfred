#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/setup.hpp"
#include "wilfred/updater/updater.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace {

// A minimal POSIX tar stream: one regular file plus the end-of-archive
// marker (two zero blocks).
std::vector<std::uint8_t> one_file_tar(const std::string& name, const std::string& content) {
  std::vector<std::uint8_t> block(512, 0);
  std::memcpy(block.data(), name.c_str(), name.size());
  std::memcpy(block.data() + 100, "0000644", 7);  // mode
  std::memcpy(block.data() + 108, "0000000", 7);  // uid
  std::memcpy(block.data() + 116, "0000000", 7);  // gid
  char size[13];
  std::snprintf(size, sizeof(size), "%011zo", content.size());
  std::memcpy(block.data() + 124, size, 12);
  std::memcpy(block.data() + 257, "ustar", 5);  // magic
  std::memcpy(block.data() + 263, "00", 2);     // version
  // Checksum: computed with the checksum field read as spaces.
  std::memcpy(block.data() + 148, "        ", 8);
  unsigned sum = 0;
  for (std::uint8_t b : block) sum += b;
  char chk[9];
  std::snprintf(chk, sizeof(chk), "%06o ", sum);
  std::memcpy(block.data() + 148, chk, 8);

  std::vector<std::uint8_t> tar = block;
  tar.insert(tar.end(), content.begin(), content.end());
  while (tar.size() % 512 != 0) tar.push_back(0);
  tar.insert(tar.end(), 1024, 0);
  return tar;
}

// RFC 1952 container: 10-byte header (with optional FNAME), one stored
// deflate block, and the 8-byte trailer inflate stops short of.
std::vector<std::uint8_t> gzip_container(const std::vector<std::uint8_t>& payload,
                                         bool with_fname) {
  std::vector<std::uint8_t> gz{0x1F, 0x8B, 0x08, with_fname ? std::uint8_t{0x08}
                                                            : std::uint8_t{0x00},
                               0x00, 0x00, 0x00, 0x00, 0x00, 0x03};
  if (with_fname) {
    const char fname[] = "wilfred-package.tar";
    gz.insert(gz.end(), fname, fname + sizeof(fname));
  }
  const std::size_t len = payload.size();
  gz.push_back(0x01);  // bfinal=1, btype=00 (stored)
  gz.push_back(static_cast<std::uint8_t>(len & 0xFF));
  gz.push_back(static_cast<std::uint8_t>((len >> 8) & 0xFF));
  gz.push_back(static_cast<std::uint8_t>((~len) & 0xFF));
  gz.push_back(static_cast<std::uint8_t>(((~len) >> 8) & 0xFF));
  gz.insert(gz.end(), payload.begin(), payload.end());
  for (int i = 0; i < 8; ++i) gz.push_back(0xFF);  // CRC32 + ISIZE
  return gz;
}

}  // namespace

void test_updater() {
  using namespace wilfred;

  auto v = parse_version("v1.2.3");
  CHECK(v.valid());
  CHECK_EQ(v.major, 1);
  CHECK_EQ(v.minor, 2);
  CHECK_EQ(v.patch, 3);
  CHECK_EQ(v.tweak, 0);

  // Multi-digit components of any width.
  auto w2 = parse_version("v10.2.3");
  CHECK(w2.valid());
  CHECK_EQ(w2.major, 10);
  auto w3 = parse_version("v123.45.67");
  CHECK(w3.valid());
  CHECK_EQ(w3.major, 123);
  CHECK_EQ(w3.minor, 45);
  CHECK_EQ(w3.patch, 67);

  // No prefix, uppercase prefix, short forms.
  auto bare = parse_version("1.2.3");
  CHECK(bare.valid());
  CHECK_EQ(bare.compare(v), 0);
  auto upper = parse_version("V1.2.3");
  CHECK(upper.valid());
  CHECK_EQ(upper.compare(v), 0);
  auto two = parse_version("v1.2");
  CHECK(two.valid());
  CHECK_EQ(two.major, 1);
  CHECK_EQ(two.minor, 2);
  CHECK_EQ(two.patch, 0);
  auto one = parse_version("v5");
  CHECK(one.valid());
  CHECK_EQ(one.major, 5);

  // Fourth component.
  auto four = parse_version("v1.2.3.4");
  CHECK(four.valid());
  CHECK_EQ(four.tweak, 4);
  CHECK(four.compare(v) > 0);
  CHECK_EQ(four.to_string(), "1.2.3.4");
  CHECK_EQ(v.to_string(), "1.2.3");

  // Whitespace and suffixes are tolerated.
  auto spaced = parse_version("  v1.2.3  ");
  CHECK(spaced.valid());
  CHECK_EQ(spaced.compare(v), 0);
  auto beta = parse_version("v1.2.3-beta");
  CHECK(beta.valid());
  CHECK_EQ(beta.compare(v), 0);
  auto build = parse_version("v1.2.3+build.1");
  CHECK(build.valid());
  CHECK_EQ(build.compare(v), 0);
  auto both = parse_version("v10.0.1-rc.1+build");
  CHECK(both.valid());
  CHECK_EQ(both.major, 10);
  CHECK_EQ(both.patch, 1);

  // Ordering.
  CHECK(parse_version("v1.2.4").compare(v) > 0);
  CHECK(parse_version("v1.2.3").compare(parse_version("v1.2.4")) < 0);
  CHECK(parse_version("v2.0.0").compare(parse_version("v10.0.0")) < 0);
  CHECK(parse_version("v1.10.0").compare(parse_version("v1.9.9")) > 0);

  // Invalid inputs.
  CHECK(!parse_version("").valid());
  CHECK(!parse_version("v").valid());
  CHECK(!parse_version("abc").valid());
  CHECK(!parse_version("v1.2.x").valid());
  CHECK(!parse_version("v1..3").valid());
  CHECK(!parse_version("v1.2.3.4.5").valid());
  auto trailing_dash = parse_version("v1.2.3-");
  CHECK(trailing_dash.valid());
  CHECK_EQ(trailing_dash.compare(v), 0);
  CHECK(!Version().valid());

  // Background-check state starts unchecked (no network in tests).
  auto pend = pending_update();
  CHECK(!pend.checked);
  CHECK(!pend.available);

  // `update` mini intent.
  CHECK(parse_mini_intent("update").kind == MiniKind::Update);
  CHECK(parse_mini_intent("updates").kind == MiniKind::Update);
  CHECK(parse_mini_intent("upgrade").kind == MiniKind::Update);
  CHECK(parse_mini_intent("upgrades").kind == MiniKind::Update);

  // Unchecked state yields the "checking" card, never empty.
  Config cfg;
  auto cards = update_results("", cfg);
  CHECK(!cards.empty());
  CHECK(cards[0].category == "update");

  // Regression: release assets ship .tar.gz for Linux/macOS/BSD. A gzip
  // container must extract: the RFC 1952 header is skipped so the raw
  // deflate stream reaches inflate_decompress().
  namespace fs = std::filesystem;
  std::error_code ec;
  auto root = fs::temp_directory_path() / "wilfred_updater_tarball";
  fs::remove_all(root, ec);

  const std::string content = "wilfred tarball update\n";
  const auto tar = one_file_tar("hello.txt", content);

  auto extract_gzip = [&](bool with_fname, const std::string& name) {
    auto archive = root / (name + ".tar.gz");
    const auto gz = gzip_container(tar, with_fname);
    {
      std::ofstream f(archive, std::ios::binary | std::ios::trunc);
      CHECK(f.write(reinterpret_cast<const char*>(gz.data()),
                    static_cast<std::streamsize>(gz.size())));
    }
    std::string dest = (root / name).string();
    std::string err;
    CHECK(extract_tarball(archive.string(), dest, &err));

    std::ifstream out(fs::path(dest) / "hello.txt", std::ios::binary);
    std::string got((std::istreambuf_iterator<char>(out)), std::istreambuf_iterator<char>());
    CHECK_EQ(got, content);
  };

  fs::create_directories(root, ec);
  extract_gzip(false, "basic");
  extract_gzip(true, "named");  // FNAME header field must be skipped too

  fs::remove_all(root, ec);
}
