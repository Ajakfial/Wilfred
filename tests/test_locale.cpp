#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/config/yaml.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/locale/locale.hpp"
#include "wilfred/search/minis.hpp"

#include <cstring>
#include <filesystem>

void test_locale() {
  using namespace wilfred;
  namespace fs = std::filesystem;

  // --- Code syntax + normalization ---
  {
    CHECK(LocaleStore::valid_code_syntax("auto"));
    CHECK(LocaleStore::valid_code_syntax("en"));
    CHECK(LocaleStore::valid_code_syntax("de"));
    CHECK(LocaleStore::valid_code_syntax("pt-BR"));
    CHECK(!LocaleStore::valid_code_syntax(""));
    CHECK(!LocaleStore::valid_code_syntax("english"));
    CHECK(!LocaleStore::valid_code_syntax("e"));
    CHECK(!LocaleStore::valid_code_syntax("de!"));
    CHECK_EQ(normalize_lang_code("de_DE.UTF-8"), "de");
    CHECK_EQ(normalize_lang_code("FR-fr"), "fr");
    CHECK_EQ(normalize_lang_code("eng"), "en");
    CHECK_EQ(normalize_lang_code("es"), "es");
    CHECK_EQ(normalize_lang_code(""), "en");
    CHECK_EQ(normalize_lang_code("C"), "en");
    CHECK(!system_language_code().empty());
  }

  // --- English fallback (no catalog loaded) ---
  {
    auto& loc = LocaleStore::instance();
    loc.configure("en");
    loc.load();
    CHECK_EQ(loc.code(), "en");
    CHECK_EQ(loc.tr("action.open"), "Open");
    CHECK_EQ(loc.tr("help.01"), "weather [city]  \u00b7  local forecast");
    CHECK_EQ(loc.tr("no.such.key"), "no.such.key");  // unknown -> key itself
    CHECK_EQ(loc.tr("overlay.bar_results_other", {{"n", "3"}}), "3 results");
    CHECK_EQ(loc.tr("pins.pinned", {{"target", "X"}}), "Pinned X");
    // Unknown placeholders are left untouched.
    CHECK_EQ(loc.tr("pins.pinned", {{"other", "X"}}), "Pinned {target}");
    auto ov = loc.overlay_strings();
    CHECK_EQ(ov.size(), 24u);
    CHECK_EQ(ov["overlay.search_placeholder"], "Search files, apps, and more");
  }

  // --- Override file merges over English ---
  {
    auto dir = fs::temp_directory_path() / "wilf-locale-test";
    std::error_code ec;
    fs::create_directories(dir, ec);
    auto p = (dir / "xx.yml").string();
    const char* text = "action.open: \"Oeffnen\"\noverlay.bar_fix: \"Fix!\"\n";
    CHECK(write_file_atomic(p, text, std::strlen(text)));
    auto& loc = LocaleStore::instance();
    loc.configure("en");
    loc.load();
    CHECK(loc.load_from(p));
    CHECK_EQ(loc.tr("action.open"), "Oeffnen");
    CHECK_EQ(loc.tr("overlay.bar_fix"), "Fix!");
    CHECK_EQ(loc.tr("action.reveal"), "Show in folder");  // fallback intact
    CHECK(!loc.load_from((dir / "missing.yml").string()));
    // Restore English for the rest of the suite.
    loc.configure("en");
    loc.load();
    fs::remove_all(dir, ec);
  }

  // --- Shipped catalogs parse and cover every key ---
  {
#ifdef WILFRED_SOURCE_DIR
    std::string root = WILFRED_SOURCE_DIR;
    auto keys = LocaleStore::english_keys();
    CHECK(!keys.empty());
    for (auto& code : LocaleStore::shipped_codes()) {
      auto path = root + "/lang/" + code + ".yml";
      std::string text;
      CHECK(read_file_all(path, text));
      YamlValue yml;
      YamlError ye;
      CHECK(parse_yaml(text, yml, ye));
      CHECK(yml.is_map());
      for (auto& k : keys) {
        auto* v = yml.get(k);
        if (!v || !v->is_string() || v->as_string().empty()) {
          CHECK(false);  // missing key printed below
          std::cerr << "missing " << k << " in " << path << "\n";
          break;
        }
      }
    }
    // German end-to-end through the real loader.
    {
      auto& loc = LocaleStore::instance();
      loc.configure("en");
      loc.load();
      CHECK(loc.load_from(root + "/lang/de.yml"));
      CHECK_EQ(loc.tr("action.open"), "\u00d6ffnen");
      CHECK_EQ(loc.tr("overlay.bar_results_other", {{"n", "3"}}), "3 Ergebnisse");
      auto help = mini_results("help", Config{}, "");
      bool german = false;
      for (auto& c : help)
        if (c.title.find("Vorhersage") != std::string::npos) german = true;
      CHECK(german);
      loc.configure("en");
      loc.load();
    }
#else
    CHECK(false);  // WILFRED_SOURCE_DIR must be defined for catalog tests
#endif
  }

  // --- Config: ui.language ---
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text("ui:\n  language: de\n", cfg, err));
    CHECK_EQ(cfg.ui.language, "de");
    CHECK(load_config_text("ui:\n  language: auto\n", cfg, err));
    CHECK(load_config_text("ui:\n  language: pt-BR\n", cfg, err));
    Config bad;
    CHECK(!load_config_text("ui:\n  language: german\n", bad, err));
    CHECK(!load_config_text("ui:\n  language: e\n", bad, err));
  }
}
