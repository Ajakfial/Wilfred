#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/config/settings.hpp"
#include "wilfred/search/setup.hpp"
#include "wilfred/ui/web_ui.hpp"

#include <unordered_set>

void test_settings() {
  using namespace wilfred;

  // --- Schema is complete, unique, and typed ---
  {
    auto& all = all_settings();
    CHECK(all.size() > 100u);
    std::unordered_set<std::string> seen;
    for (auto& m : all) {
      CHECK(seen.insert(m.key).second);  // no duplicates
      CHECK(!m.key.empty() && m.key != ".");
      if (m.key != "pins") CHECK(m.key.find('.') != std::string::npos);
      if (!m.options.empty()) CHECK(m.type == SettingType::Str);
      SettingMeta back;
      CHECK(setting_meta(m.key, back));
      CHECK_EQ(back.key, m.key);
    }
    SettingMeta junk;
    CHECK(!setting_meta("nope.nope", junk));
    CHECK(!setting_meta("search", junk));
    // Spot-check flags reviewers care about.
    SettingMeta meta{"", SettingType::Str, false, false, {}};
    CHECK(setting_meta("sync.password", meta) && meta.secret && !meta.live);
    CHECK(setting_meta("ui.accent", meta) && !meta.secret && meta.live);
    CHECK(setting_meta("search.max_results", meta) && !meta.secret && !meta.live);
  }

  // --- Every schema key validates through the real config loader ---
  {
    auto sample_doc = [](const SettingMeta& m) -> std::string {
      auto scalar = [&](const std::string& v) {
        auto section = m.key.substr(0, m.key.find('.'));
        auto key = m.key.substr(m.key.find('.') + 1);
        return section + ":\n  " + key + ": " + v + "\n";
      };
      if (m.key == "pins") return "pins:\n  - alpha\n  - beta\n";
      if (m.key == "sync.encrypt") return "sync:\n  encrypt: true\n  password: x\n";
      if (m.type == SettingType::Bool) return scalar("true");
      if (m.type == SettingType::Double) return scalar("0.5");
      if (m.type == SettingType::List) return scalar("alpha, beta");
      if (m.type == SettingType::Int) {
        if (m.key == "index.cpu_percent_limit") return scalar("45");
        if (m.key == "index.memory_limit_mb") return scalar("384");
        if (m.key == "ui.width") return scalar("720");
        if (m.key == "embedding.dim") return scalar("384");
        if (m.key == "plugins.timeout_ms") return scalar("400");
        if (m.key == "remotes.timeout_ms") return scalar("5000");
        if (m.key == "ai.timeout_ms") return scalar("30000");
        if (m.key == "packages.timeout_ms") return scalar("8000");
        if (m.key == "os_search.timeout_ms") return scalar("3000");
        return scalar("8");
      }
      if (m.key == "logging.level") return scalar("info");
      if (m.key == "browser.search_template") return scalar("https://x.example/?q={query}");
      if (m.key == "plugins.registry") return scalar("https://x.example/r.json");
      if (m.key == "ui.language") return scalar("de");
      if (m.key == "index.format") return scalar("auto");
      if (m.key == "transcription.language") return scalar("en");
      if (m.key == "embedding.backend") return scalar("hash");
      if (m.key == "ai.provider") return scalar("openai");
      if (m.key == "providers.semantic_backend") return scalar("hybrid");
      if (m.key == "os_search.backend") return scalar("auto");
      if (m.key == "hotkey.key") return scalar("G");
      return scalar("x");
    };
    for (auto& m : all_settings()) {
      Config cfg;
      ConfigError err;
      bool ok = load_config_text(sample_doc(m), cfg, err);
      if (!ok) {
        std::cerr << "schema key rejected: " << m.key << " (" << err.message << ")\n";
        CHECK(false);
      }
      // And the typed editor agrees on the type.
      SettingMeta back;
      CHECK(setting_meta(m.key, back));
      CHECK(back.type == m.type);
    }
  }

  // --- Getters cover the schema (secrets stay empty) ---
  {
    Config cfg;  // defaults
    for (auto& m : all_settings()) {
      std::string out, err;
      bool ok = config_get_value(cfg, m.key, out, err);
      if (!ok) {
        std::cerr << "unreadable schema key: " << m.key << " (" << err << ")\n";
        CHECK(false);
      }
      if (m.secret) CHECK(out.empty());
    }
    std::string out, err;
    CHECK(config_get_value(cfg, "ui.language", out, err) && out == "auto");
    CHECK(config_get_value(cfg, "index.format", out, err) && out == "auto");
    CHECK(config_get_value(cfg, "ranking.pinned", out, err) && out == "900");
    CHECK(config_get_value(cfg, "sync.password", out, err) && out.empty());
    CHECK(!config_get_value(cfg, "bogus.key", out, err));
    Config custom;
    ConfigError cerr;
    CHECK(load_config_text(
        "ui:\n  accent: \"#ff5500\"\n  font_size: 18\n  language: fr\nsync:\n  encrypt: true\n"
        "  password: s3cret\nindex:\n  format: v2\nranking:\n  pinned: 1\npins:\n  - firefox\n",
        custom, cerr));
    CHECK(config_get_value(custom, "ui.accent", out, err) && out == "#ff5500");
    CHECK(config_get_value(custom, "ui.font_size", out, err) && out == "18");
    CHECK(config_get_value(custom, "ui.language", out, err) && out == "fr");
    CHECK(config_get_value(custom, "sync.encrypt", out, err) && out == "true");
    CHECK(config_get_value(custom, "index.format", out, err) && out == "v2");
    CHECK(config_get_value(custom, "ranking.pinned", out, err) && out == "1");
    CHECK(config_get_value(custom, "pins", out, err) && out == "firefox");
  }

  // --- Settings JSON shape (pure seam, no real config files touched) ---
  {
    Config cfg;
    auto js = overlay_settings_json_from(cfg);
    CHECK(js.find("\"type\":\"settings\"") != std::string::npos);
    CHECK(js.find("\"key\":\"search.max_results\"") != std::string::npos);
    CHECK(js.find("\"key\":\"ui.language\"") != std::string::npos);
    CHECK(js.find("\"key\":\"ranking.pinned\"") != std::string::npos);
    CHECK(js.find("\"type\":\"bool\"") != std::string::npos);
    CHECK(js.find("\"secret\":true") != std::string::npos);  // password/token/api_key
    CHECK(js.find("\"live\":true") != std::string::npos);    // ui.*
    CHECK(js.find("\"options\":[\"dark\",\"light\"]") != std::string::npos);
    CHECK(js.find("\"search.max_results\":\"40\"") != std::string::npos);
    CHECK(js.find("\"ui.language\":\"auto\"") != std::string::npos);
    // Secrets never leak values.
    CHECK(js.find("\"sync.password\":\"\"") != std::string::npos);
  }
}
