#include "test.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/pkg.hpp"

void test_packages() {
  using namespace wilfred;
  // Manager ids.
  CHECK(pkg_is_manager("winget"));
  CHECK(pkg_is_manager("BREW"));
  CHECK(pkg_is_manager("Apt"));
  CHECK(!pkg_is_manager("firefox"));
  CHECK(!pkg_is_manager("pkg"));
  CHECK(!pkg_is_manager(""));
  CHECK_EQ(pkg_known_managers().size(), 6u);

  // Intent parsing.
  {
    auto it = pkg_parse_intent("winget firefox");
    CHECK(it.matched && it.manager == "winget" && it.query == "firefox");
  }
  {
    auto it = pkg_parse_intent("  BREW   Firefox  ");
    CHECK(it.matched && it.manager == "brew" && it.query == "Firefox");
  }
  {
    auto it = pkg_parse_intent("pkg apt vlc");
    CHECK(it.matched && it.manager == "apt" && it.query == "vlc");
  }
  CHECK(!pkg_parse_intent("firefox").matched);
  CHECK(!pkg_parse_intent("winget").matched);
  CHECK(!pkg_parse_intent("winget ").matched);
  CHECK(!pkg_parse_intent("pkg brew").matched);
  CHECK(!pkg_parse_intent("pkg foo bar").matched);
  CHECK(!pkg_parse_intent("").matched);

  // winget table.
  {
    std::string out =
        "Name              Id                    Version   Source\n"
        "-------------------------------------------------------\n"
        "Mozilla Firefox   Mozilla.Firefox       131.0     winget\n"
        "Firefox ESR       Mozilla.Firefox.ESR   128.3     winget\n"
        "No package found matching input criteria.\n";
    auto hits = pkg_parse_winget(out);
    CHECK_EQ(hits.size(), 2u);
    CHECK_EQ(hits[0].id, "Mozilla.Firefox");
    CHECK_EQ(hits[0].name, "Mozilla Firefox");
    CHECK_EQ(hits[0].version, "131.0");
    CHECK_EQ(hits[1].id, "Mozilla.Firefox.ESR");
  }
  CHECK(pkg_parse_winget("No packages found.\n").empty());

  // brew lists (formula + cask outputs are bare names).
  {
    std::string out = "==> Formulae\nfirefox\nfirefox@esr\nWarning: some warning\n";
    auto hits = pkg_parse_brew(out);
    CHECK_EQ(hits.size(), 2u);
    CHECK_EQ(hits[0].id, "firefox");
  }

  // apt pairs.
  {
    std::string out =
        "firefox/131.0 amd64\n"
        "  Safe and easy web browser\n"
        "chromium/2:130.0 amd64\n"
        "  web browser\n";
    auto hits = pkg_parse_apt(out);
    CHECK_EQ(hits.size(), 2u);
    CHECK_EQ(hits[0].id, "firefox");
    CHECK_EQ(hits[0].version, "131.0");
    CHECK_EQ(hits[1].version, "2:130.0");
  }

  // choco pipe format.
  {
    auto hits = pkg_parse_choco("firefox|131.0\nvlc|3.0.20\n");
    CHECK_EQ(hits.size(), 2u);
    CHECK_EQ(hits[1].id, "vlc");
    CHECK_EQ(hits[1].version, "3.0.20");
  }

  // flatpak table.
  {
    std::string out =
        "Name                 Description            Application ID          Version   Branch   "
        "Remotes\n"
        "Firefox Web Browser  Safe and easy browser  org.mozilla.firefox  131.0     stable   "
        "flathub\n";
    auto hits = pkg_parse_flatpak(out);
    CHECK_EQ(hits.size(), 1u);
    CHECK_EQ(hits[0].id, "org.mozilla.firefox");
    CHECK_EQ(hits[0].source, "flathub");
  }

  // pacman -Ss.
  {
    std::string out = "extra/firefox 131.0-1\n    Safe and easy web browser\n";
    auto hits = pkg_parse_pacman(out);
    CHECK_EQ(hits.size(), 1u);
    CHECK_EQ(hits[0].id, "firefox");
    CHECK_EQ(hits[0].source, "extra");
  }

  // Install command rendering.
  CHECK_EQ(pkg_install_command("winget", "Mozilla.Firefox"),
           "winget install --exact --id Mozilla.Firefox");
  CHECK_EQ(pkg_install_command("brew", "firefox"), "brew install firefox");
  CHECK_EQ(pkg_install_command("apt", "vlc"), "sudo apt install -y vlc");
  CHECK_EQ(pkg_install_command("flatpak", "org.mozilla.firefox", "flathub"),
           "flatpak install -y flathub org.mozilla.firefox");

  // Mini intents for the manager list.
  CHECK(parse_mini_intent("packages").kind == MiniKind::Packages);
  CHECK(parse_mini_intent("package brew").kind == MiniKind::Packages);
  CHECK(parse_mini_intent("pkg").kind == MiniKind::Packages);

  // Config: valid block.
  {
    wilfred::Config cfg;
    wilfred::ConfigError err;
    CHECK(
        load_config_text("packages:\n  enabled: true\n  max_results: 5\n  timeout_ms: 3000\n"
                         "  managers: [brew, apt]\n",
                         cfg, err));
    CHECK(cfg.packages.enabled);
    CHECK_EQ(cfg.packages.max_results, 5);
    CHECK_EQ(cfg.packages.timeout_ms, 3000);
    CHECK_EQ(cfg.packages.managers.size(), 2u);
  }
  // Config: defaults stay enabled with no managers.
  {
    wilfred::Config cfg;
    wilfred::ConfigError err;
    CHECK(load_config_text("search:\n  max_results: 40\n", cfg, err));
    CHECK(cfg.packages.enabled);
    CHECK(cfg.packages.managers.empty());
  }
  // Config: unknown manager rejected.
  {
    wilfred::Config bad;
    wilfred::ConfigError err;
    CHECK(!load_config_text("packages:\n  managers: [nope]\n", bad, err));
    CHECK(!err.message.empty());
  }
  // Config: ranges enforced.
  {
    wilfred::Config bad;
    wilfred::ConfigError err;
    CHECK(!load_config_text("packages:\n  max_results: 99\n", bad, err));
    CHECK(!load_config_text("packages:\n  timeout_ms: 500\n", bad, err));
  }
}
