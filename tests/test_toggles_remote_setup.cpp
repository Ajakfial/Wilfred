#include "test.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/plugin/mini.hpp"
#include "wilfred/plugin/registry.hpp"
#include "wilfred/plugin/trust.hpp"
#include "wilfred/search/layouts.hpp"
#include "wilfred/search/minis.hpp"
#include "wilfred/search/remote.hpp"
#include "wilfred/search/setup.hpp"
#include "wilfred/search/toggles.hpp"

void test_toggles_remote_setup() {
  using namespace wilfred;
  // Toggle arg parsing.
  CHECK(parse_toggle_arg("") == ToggleOp::Status);
  CHECK(parse_toggle_arg("on") == ToggleOp::On);
  CHECK(parse_toggle_arg("OFF") == ToggleOp::Off);
  CHECK(parse_toggle_arg("toggle") == ToggleOp::Toggle);
  CHECK(parse_toggle_arg("garbage") == ToggleOp::Status);

  // Volume parsing.
  {
    std::string kind;
    int level = -1;
    CHECK(parse_volume_arg("", kind, level) && kind == "status");
    CHECK(parse_volume_arg("50", kind, level) && kind == "set" && level == 50);
    CHECK(parse_volume_arg("120", kind, level) && kind == "set" && level == 100);
    CHECK(parse_volume_arg("mute", kind, level) && kind == "mute");
    CHECK(parse_volume_arg("unmute", kind, level) && kind == "unmute");
    CHECK(parse_volume_arg("up", kind, level) && kind == "up");
    CHECK(parse_volume_arg("down", kind, level) && kind == "down");
  }
  // Brightness parsing.
  {
    std::string kind;
    int level = -1;
    CHECK(parse_brightness_arg("", kind, level) && kind == "status");
    CHECK(parse_brightness_arg("70%", kind, level) && kind == "set" && level == 70);
    CHECK(parse_brightness_arg("up", kind, level) && kind == "up");
  }
  // Settings pages.
  CHECK_EQ(normalize_settings_page(""), "");
  CHECK_EQ(normalize_settings_page("wifi"), "wifi");
  CHECK_EQ(normalize_settings_page("WI-FI"), "wifi");
  CHECK_EQ(normalize_settings_page("bluetooth"), "bluetooth");
  CHECK_EQ(normalize_settings_page("sound"), "sound");
  CHECK(!settings_page_list().empty());

  // Mini intents.
  CHECK(parse_mini_intent("wifi").kind == MiniKind::Wifi);
  CHECK(parse_mini_intent("wifi off").kind == MiniKind::Wifi);
  CHECK(parse_mini_intent("bluetooth on").kind == MiniKind::Bluetooth);
  CHECK(parse_mini_intent("volume 50").kind == MiniKind::Volume);
  CHECK(parse_mini_intent("brightness 70").kind == MiniKind::Brightness);
  CHECK(parse_mini_intent("settings wifi").kind == MiniKind::Settings);
  CHECK(parse_mini_intent("settings").kind == MiniKind::Settings);
  CHECK(parse_mini_intent("setup").kind == MiniKind::Setup);
  CHECK(parse_mini_intent("wizard").kind == MiniKind::Setup);
  CHECK(parse_mini_intent("config validate").kind == MiniKind::Config);

  // Remote URL templating.
  CHECK_EQ(remote_url_for("https://x.example/s?q={query}", "a b"), "https://x.example/s?q=a b");
  {
    auto u = remote_url_for("https://x.example/s?q={query_enc}", "a b");
    CHECK(u.find("a%20b") != std::string::npos || u.find("a+b") != std::string::npos ||
          u.find("a%2") != std::string::npos);
  }
  {
    auto u = remote_url_for("https://x.example/s", "hi");
    CHECK(u.find("hi") != std::string::npos && u.find('?') != std::string::npos);
  }
  // Remote response parsing.
  {
    std::string body =
        R"({"results":[{"title":"T","subtitle":"S","url":"https://e.com/1","score":600}]})";
    auto rs = remote_parse_response(body, "wiki", "q");
    CHECK_EQ(rs.size(), 1u);
    CHECK_EQ(rs[0].title, "T");
    CHECK_EQ(rs[0].category, "remote");
  }
  {
    auto rs = remote_parse_response("not json", "wiki", "q");
    CHECK(rs.empty());
  }

  // Config: remotes opt-in parsing.
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text("remotes:\n  enabled: true\n  timeout_ms: 3000\n  sources:\n    - name: wiki\n      url: \"https://example.com/s?q={query_enc}\"\n",
                           cfg, err));
    CHECK(cfg.remotes.enabled);
    CHECK_EQ(cfg.remotes.sources.size(), 1u);
    CHECK_EQ(cfg.remotes.timeout_ms, 3000);
    Config bad;
    CHECK(!load_config_text("remotes:\n  enabled: true\n  sources:\n    - name: x\n      url: ftp://bad\n",
                            bad, err));
    // Disabled by default.
    Config def;
    CHECK(load_config_text("search:\n  max_results: 40\n", def, err));
    CHECK(!def.remotes.enabled);
    CHECK(def.remotes.sources.empty());
  }

  // Setup validation + summary (no hardware).
  {
    std::string msg;
    CHECK(validate_config_text("search:\n  max_results: 40\n", msg));
    CHECK(!validate_config_text("search:\n  max_results: 9999\n", msg));
    CHECK(!msg.empty());
  }
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text("search:\n  max_results: 40\n", cfg, err));
    CHECK(!setup_summary(cfg).empty());
  }
  // Mini builders run without crashing even when hardware is absent.
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text("search:\n  max_results: 40\n", cfg, err));
    CHECK(!wifi_results("", cfg).empty());
    CHECK(!bluetooth_results("", cfg).empty());
    CHECK(!volume_results("", cfg).empty());
    CHECK(!brightness_results("", cfg).empty());
    CHECK(!settings_results("", cfg).empty());
    CHECK(!setup_results("", cfg).empty());
    CHECK(!config_results("", cfg).empty());
    CHECK(parse_mini_intent("plugins").kind == MiniKind::Plugins);
    CHECK(parse_mini_intent("tile halves").kind == MiniKind::Layout);
    CHECK(!plugin_results("", cfg).empty());
  }
  // Remotes: headers + per-source max_results (in-memory parse only).
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text(
        "remotes:\n  enabled: true\n  sources:\n    - name: wiki\n"
        "      url: \"https://example.com/s?q={query_enc}\"\n"
        "      max_results: 3\n"
        "      headers:\n"
        "        Authorization: \"Bearer abc\"\n",
        cfg, err));
    CHECK(cfg.remotes.enabled);
    CHECK_EQ(cfg.remotes.sources.size(), 1u);
    CHECK_EQ(cfg.remotes.sources[0].max_results, 3);
    CHECK_EQ(cfg.remotes.sources[0].headers.count("Authorization"), 1u);
    Config bad;
    CHECK(!load_config_text(
        "remotes:\n  enabled: true\n  sources:\n    - name: x\n"
        "      url: \"https://example.com/s\"\n"
        "      headers: not-a-map\n",
        bad, err));
  }
  // Plugins: registry/trust config + pure trust logic (no file writes).
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text(
        "plugins:\n  enabled: true\n  registry: \"https://example.com/plugins.json\"\n"
        "  require_approval: false\n",
        cfg, err));
    CHECK_EQ(cfg.plugins.registry, "https://example.com/plugins.json");
    CHECK(!cfg.plugins.require_approval);
    Config def;
    CHECK(load_config_text("search:\n  max_results: 40\n", def, err));
    CHECK(def.plugins.require_approval);  // TOFU on by default.
    Config bad;
    CHECK(!load_config_text("plugins:\n  registry: ftp://bad\n", bad, err));
  }
  {
    auto entries = plugin_registry_parse(
        R"({"plugins":[{"id":"demo","version":"1.0","kind":"stdio",)"
        R"("url":"https://example.com/demo","sha256":"abc","description":"d",)"
        R"("permissions":["network"]}]})");
    CHECK_EQ(entries.size(), 1u);
    CHECK_EQ(entries[0].id, "demo");
    CHECK_EQ(entries[0].permissions.size(), 1u);
    CHECK(plugin_registry_parse("garbage").empty());
    // Trust: hash mismatch and permission drift both deny.
    std::vector<TrustEntry> trust = {{"demo", "abc", {"network"}, true}};
    CHECK(plugin_is_approved(trust, "demo", "abc", {"network"}));
    CHECK(!plugin_is_approved(trust, "demo", "different", {"network"}));
    CHECK(!plugin_is_approved(trust, "demo", "abc", {"network", "fs"}));
    CHECK(!plugin_is_approved(trust, "other", "abc", {"network"}));
    CHECK_EQ(plugin_fingerprint("demo", "ABC", ""), "abc");
  }
  // Settings editor: typed get on an in-memory config (no file writes).
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text(
        "search:\n  max_results: 12\nbrowser:\n  search_template: "
        "\"https://example.com/q={query}\"\n",
        cfg, err));
    std::string out, msg;
    CHECK(config_get_value(cfg, "search.max_results", out, msg) && out == "12");
    CHECK(config_get_value(cfg, "browser.search_template", out, msg) &&
          out == "https://example.com/q={query}");
    CHECK(!config_get_value(cfg, "nope.nope", out, msg));
    CHECK(!config_get_value(cfg, "workflows", out, msg));
  }
  // Layouts config + tiling presets (pure; no windows touched).
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text(
        "layouts:\n  auto_apply: true\n  auto_layout: work\n  monitor_layouts:\n    docked: work\n",
        cfg, err));
    CHECK(cfg.layouts.auto_apply);
    CHECK_EQ(cfg.layouts.auto_layout, "work");
    CHECK_EQ(cfg.layouts.monitor_layouts.count("docked"), 1u);
    CHECK(!LayoutStore::tiling_preset_names().empty());
    Layout lay;
    lay.name = "work";
    LayoutEntry e;
    e.match = "code";
    e.x = 1;
    e.y = 2;
    e.w = 3;
    e.h = 4;
    lay.entries.push_back(e);
    auto blob = LayoutStore::serialize(lay);
    Layout back;
    CHECK(LayoutStore::parse(blob, back));
    CHECK_EQ(back.entries.size(), 1u);
    CHECK_EQ(back.entries[0].x, 1);
  }
}
