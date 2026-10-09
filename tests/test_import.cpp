#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/import/import.hpp"

using namespace wilfred;
using namespace wilfred::import;

void test_import() {
  // slugify / normalize / hotkey helpers
  CHECK_EQ(slugify_keyword("GitHub Search"), "githubsearch");
  CHECK_EQ(slugify_keyword("  G  "), "g");
  CHECK(normalize_url_template("https://www.google.com/search?q=%s") ==
        "https://www.google.com/search?q={query}");
  CHECK(normalize_url_template("https://x.test/?q={q}") == "https://x.test/?q={query}");
  CHECK(normalize_url_template("https://x.test/?q={searchTerms}") == "https://x.test/?q={query}");
  CHECK(normalize_url_template("https://x.test/search?q=\\{@}") ==
        "https://x.test/search?q={query}");
  CHECK(looks_like_search_url("https://www.google.com/search?q=%s"));
  CHECK(!looks_like_search_url("https://example.com/static"));

  ImportHotkey hk;
  CHECK(parse_hotkey_string("Alt+Space", hk));
  CHECK(hk.has);
  CHECK_EQ(hk.key, "Space");
  CHECK_EQ(hk.modifiers.size(), 1u);
  CHECK_EQ(hk.modifiers[0], "alt");

  CHECK(parse_hotkey_string("Ctrl+Alt+W", hk));
  CHECK_EQ(hk.key, "W");
  CHECK_EQ(hk.modifiers.size(), 2u);

  CHECK(parse_hotkey_string("<Primary>space", hk));
  CHECK_EQ(hk.modifiers[0], "ctrl");
  CHECK_EQ(hk.key, "Space");

  CHECK(parse_hotkey_string("Command+Space", hk));
  CHECK(hk.has);

  CHECK(!parse_hotkey_string("", hk));
  CHECK(!parse_hotkey_string("   ", hk));

  // supported list covers all OSes
  {
    auto all = supported_launchers();
    CHECK(all.size() >= 10u);
    CHECK(find_launcher("alfred") != nullptr);
    CHECK(find_launcher("raycast") != nullptr);
    CHECK(find_launcher("powertoys") != nullptr);
    CHECK(find_launcher("flowlauncher") != nullptr);
    CHECK(find_launcher("wox") != nullptr);
    CHECK(find_launcher("keypirinha") != nullptr);
    CHECK(find_launcher("listary") != nullptr);
    CHECK(find_launcher("ulauncher") != nullptr);
    CHECK(find_launcher("albert") != nullptr);
    CHECK(find_launcher("krunner") != nullptr);
    CHECK(find_launcher("rofi") != nullptr);
    CHECK(find_launcher("flow") != nullptr);  // alias
    bool has_mac = false, has_win = false, has_linux = false;
    for (auto& li : all) {
      if (li.os.find("macOS") != std::string::npos) has_mac = true;
      if (li.os.find("Windows") != std::string::npos) has_win = true;
      if (li.os.find("Linux") != std::string::npos) has_linux = true;
    }
    CHECK(has_mac && has_win && has_linux);
  }

  // detect by path
  CHECK_EQ(detect_id_for_path("Settings.json", "{\"SearchSources\":[]}"), "flowlauncher");
  CHECK_EQ(detect_id_for_path("Alfred.alfredpreferences/prefs.plist", "<plist></plist>"), "alfred");
  CHECK_EQ(detect_id_for_path("config.rasi", "configuration {}"), "rofi");

  std::vector<std::string> w;

  // Flow Launcher
  {
    w.clear();
    std::string flow = R"({
      "Hotkey": "Alt+Space",
      "Theme": "Dark",
      "PluginSettings": {"WebSearch": {"SearchSources": [
        {"Name": "Google", "Url": "https://www.google.com/search?q={q}", "ActionKeyword": "g"},
        {"Name": "GitHub", "Url": "https://github.com/search?q={q}", "ActionKeyword": "gh"}
      ], "DefaultSearch": "Google"}}
    })";
    auto s = parse_text("flowlauncher", flow, "Settings.json", w);
    CHECK_EQ(s.searches.size(), 2u);
    CHECK_EQ(s.searches[0].keyword, "g");
    CHECK(s.searches[0].url.find("{query}") != std::string::npos);
    CHECK(s.hotkey.has);
    CHECK_EQ(s.hotkey.key, "Space");
    CHECK(s.theme && *s.theme == "dark");
    CHECK(s.default_search_template.has_value());
  }

  // Wox shares the layout
  {
    w.clear();
    std::string wox = R"({"Hotkey":"Ctrl+Alt+W","SearchSources":[
      {"Name":"Bing","Url":"https://www.bing.com/search?q=%s","ActionKeyword":"b"}]})";
    auto s = parse_text("wox", wox, "Settings.json", w);
    CHECK_EQ(s.searches.size(), 1u);
    CHECK_EQ(s.searches[0].keyword, "b");
  }

  // PowerToys
  {
    w.clear();
    std::string pt = R"({
      "properties": {"open_powerlauncher": {"value": {
        "alt": true, "ctrl": false, "shift": false, "win": false, "action_key": "Space"}}},
      "SearchSources": [{"Name": "Duck", "Url": "https://duckduckgo.com/?q=%s", "ActionKeyword": "d"}]
    })";
    auto s = parse_text("powertoys", pt, "settings.json", w);
    CHECK(s.hotkey.has);
    CHECK_EQ(s.hotkey.key, "Space");
    CHECK(!s.searches.empty());
  }

  // PowerToys current format: {"alt":true,...,"code":32,"key":""}
  {
    w.clear();
    std::string pt =
        R"({"properties":{"open_powerlauncher":{"win":false,"ctrl":false,"alt":true,"shift":false,"code":32,"key":""}}})";
    auto s = parse_text("powertoys", pt, "settings.json", w);
    CHECK(s.hotkey.has);
    CHECK_EQ(s.hotkey.key, "Space");
    CHECK_EQ(s.hotkey.modifiers.size(), 1u);
    CHECK_EQ(s.hotkey.modifiers[0], "alt");
  }

  // Ulauncher shortcuts + settings
  {
    w.clear();
    std::string ul = R"([
      {"id":"1","name":"Google","keyword":"g","cmd":"https://www.google.com/search?q=%s","is_default_search":true},
      {"id":"2","name":"Notes","keyword":"notes","cmd":"gedit ~/notes.txt"}
    ])";
    auto s = parse_text("ulauncher", ul, "shortcuts.json", w);
    CHECK_EQ(s.searches.size(), 1u);
    CHECK_EQ(s.searches[0].keyword, "g");
    CHECK_EQ(s.quicklinks.count("notes"), 1u);
    CHECK(s.default_search_template.has_value());
  }
  {
    w.clear();
    std::string us = R"({"hotkey-show-app":"<Primary>space","theme-name":"dark"})";
    auto s = parse_text("ulauncher", us, "settings.json", w);
    CHECK(s.hotkey.has);
    CHECK(s.theme && *s.theme == "dark");
  }

  // Alfred plist
  {
    w.clear();
    std::string plist = R"(<?xml version="1.0"?>
<plist version="1.0"><dict>
<key>customSearches</key><array><dict>
<key>keyword</key><string>g</string>
<key>text</key><string>Google</string>
<key>url</key><string>https://www.google.com/search?q={query}</string>
</dict><dict>
<key>keyword</key><string>gh</string>
<key>text</key><string>GitHub</string>
<key>url</key><string>https://github.com/search?q={query}</string>
</dict></array></dict></plist>)";
    auto s = parse_text("alfred", plist, "prefs.plist", w);
    CHECK_EQ(s.searches.size(), 2u);
    CHECK_EQ(s.searches[0].keyword, "g");
    CHECK_EQ(s.searches[1].keyword, "gh");
  }
  {
    w.clear();
    std::string snip = R"(<plist><dict><dict>
<key>keyword</key><string>addr</string>
<key>snippet</key><string>123 Main St</string>
<key>text</key><string>Address</string>
</dict></dict></plist>)";
    auto s = parse_text("alfred", snip, "snippets.plist", w);
    CHECK_EQ(s.snippets.size(), 1u);
    CHECK_EQ(s.snippets[0].trigger, "addr");
  }

  // Raycast
  {
    w.clear();
    std::string rc = R"([{"name":"GitHub Search","link":"https://github.com/search?q={query}"},
      {"name":"Docs","link":"https://example.com/docs/{query}"}])";
    auto s = parse_text("raycast", rc, "quicklinks.json", w);
    CHECK_EQ(s.searches.size(), 2u);
    CHECK(!s.searches[0].keyword.empty());
  }

  // Keypirinha INI
  {
    w.clear();
    std::string ini =
        "[app]\nhotkey_run = Ctrl+Space\n[profile/Google]\n"
        "url = https://www.google.com/search?q={q}\nkeyword = g\n";
    auto s = parse_text("keypirinha", ini, "Keypirinha.ini", w);
    CHECK(s.hotkey.has);
    CHECK_EQ(s.searches.size(), 1u);
    CHECK_EQ(s.searches[0].keyword, "g");
  }

  // Listary
  {
    w.clear();
    std::string lj = R"({"launcherHotkey":"Alt+Space","keywords":[
      {"keyword":"g","url":"https://www.google.com/search?q={query}","title":"Google"}]})";
    auto s = parse_text("listary", lj, "Preferences.json", w);
    CHECK(s.hotkey.has);
    CHECK_EQ(s.searches.size(), 1u);
  }

  // Albert engines
  {
    w.clear();
    std::string eng =
        R"([{"name":"Google","trigger":"g ","url":"https://www.google.com/search?q={query}"}])";
    auto s = parse_text("albert", eng, "engines.json", w);
    CHECK_EQ(s.searches.size(), 1u);
    CHECK_EQ(s.searches[0].keyword, "g");
  }

  // KRunner
  {
    w.clear();
    std::string kr =
        "[Google]\nQuery=https://www.google.com/search?q=\\{@}\n"
        "Keyword=g\n";
    auto s = parse_text("krunner", kr, "kuriikwsfilterrc", w);
    CHECK_EQ(s.searches.size(), 1u);
    CHECK(s.searches[0].url.find("{query}") != std::string::npos);
  }

  // Rofi
  {
    w.clear();
    std::string rasi = "configuration {\n  modi: \"drun,run\";\n  theme: \"Arc-Dark\";\n}";
    auto s = parse_text("rofi", rasi, "config.rasi", w);
    CHECK(s.theme && *s.theme == "dark");
  }

  // apply merge vs overwrite
  {
    Config cfg;
    ImportOptions opts;
    opts.overwrite = false;
    ImportedSettings in;
    in.searches.push_back({"Google", "g", "https://www.google.com/search?q={query}", "test"});
    std::vector<std::string> ww;
    auto c = apply_settings(cfg, in, opts, ww);
    CHECK_EQ(c.macros_added, 1);
    CHECK_EQ(cfg.macros["g"], "https://www.google.com/search?q={query}");
    CHECK_EQ(cfg.quicklinks["g"], "https://www.google.com/search?q={query}");

    // Re-apply same: skipped, not duplicated.
    ImportCounts c2 = apply_settings(cfg, in, opts, ww);
    CHECK_EQ(c2.macros_skipped, 1);

    // Conflicting template without overwrite: kept.
    ImportedSettings in2;
    in2.searches.push_back({"Google", "g", "https://example.com/?q={query}", "test"});
    ImportCounts c3 = apply_settings(cfg, in2, opts, ww);
    CHECK_EQ(c3.macros_skipped, 1);
    CHECK_EQ(cfg.macros["g"], "https://www.google.com/search?q={query}");

    // With overwrite: replaced.
    opts.overwrite = true;
    ImportCounts c4 = apply_settings(cfg, in2, opts, ww);
    CHECK_EQ(c4.macros_overwritten, 1);
    CHECK_EQ(cfg.macros["g"], "https://example.com/?q={query}");
  }

  // snippets + hotkey + theme apply
  {
    Config cfg;
    ImportOptions opts;
    ImportedSettings in;
    in.snippets.push_back({"sig", "Sig", "Best", "test"});
    in.hotkey.has = true;
    in.hotkey.modifiers = {"ctrl", "alt"};
    in.hotkey.key = "Space";
    in.theme = "light";
    in.default_search_template = "https://www.google.com/search?q={query}";
    std::vector<std::string> ww;
    auto c = apply_settings(cfg, in, opts, ww);
    CHECK_EQ(c.snippets_added, 1);
    CHECK(c.hotkey_applied);
    CHECK(c.theme_applied);
    CHECK(c.browser_applied);
    CHECK_EQ(cfg.hotkey.key, "Space");
    CHECK_EQ(cfg.ui.theme, "light");
  }

  // serialize round-trip
  {
    Config cfg;
    cfg.macros["g"] = "https://www.google.com/search?q={query}";
    cfg.quicklinks["gh"] = "https://github.com/search?q={query}";
    cfg.snippets.items["sig"] = "Best regards\nDemo";
    cfg.hotkey.modifiers = {"ctrl", "alt"};
    cfg.hotkey.key = "Space";
    std::string text = serialize_config(cfg);
    Config back;
    ConfigError err;
    CHECK(load_config_text(text, back, err));
    CHECK_EQ(back.macros["g"], cfg.macros["g"]);
    CHECK_EQ(back.quicklinks["gh"], cfg.quicklinks["gh"]);
    CHECK_EQ(back.snippets.items["sig"], cfg.snippets.items["sig"]);
    CHECK_EQ(back.hotkey.key, "Space");
  }
}
