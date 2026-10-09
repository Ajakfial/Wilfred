#include "test.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/search/os_search.hpp"

void test_os_search() {
  using namespace wilfred;
  // Backend ids + aliases.
  CHECK_EQ(os_search_normalize_backend("auto"), "auto");
  CHECK_EQ(os_search_normalize_backend("Spotlight"), "spotlight");
  CHECK_EQ(os_search_normalize_backend("mdfind"), "spotlight");
  CHECK_EQ(os_search_normalize_backend("Windows-Search"), "windows_search");
  CHECK_EQ(os_search_normalize_backend("tracker3"), "tracker");
  CHECK_EQ(os_search_normalize_backend("baloosearch"), "baloo");
  CHECK_EQ(os_search_normalize_backend("plocate"), "locate");
  CHECK_EQ(os_search_normalize_backend("es"), "everything");
  for (auto& b : os_search_backends())
    CHECK(!b.empty());
  CHECK(!os_search_backend_label("spotlight").empty());
  CHECK(!os_search_backend_label("windows_search").empty());
  CHECK_EQ(os_search_backend_label("spotlight"), "Spotlight");
  CHECK_EQ(os_search_backend_label("windows_search"), "Windows Search");

  // Gate: needs at least 2 content chars, rejects controls/oversize.
  CHECK(!os_search_should_query(""));
  CHECK(!os_search_should_query("a"));
  CHECK(!os_search_should_query("  "));
  CHECK(os_search_should_query("fi"));
  CHECK(os_search_should_query("firefox"));
  CHECK(os_search_should_query("  Fire Fox  "));
  CHECK(!os_search_should_query(std::string("a\x01b", 3)));
  CHECK(!os_search_should_query(std::string(300, 'x')));

  // Explicit `os ` prefix.
  {
    std::string r;
    CHECK(os_search_strip_prefix("os firefox", r) && r == "firefox");
    CHECK(os_search_strip_prefix("OS:  quarterly report", r) && r == "quarterly report");
    CHECK(!os_search_strip_prefix("firefox", r));
    CHECK(!os_search_strip_prefix("os", r));
    CHECK(!os_search_strip_prefix("os ", r));
    CHECK(!os_search_strip_prefix("ossify", r));
  }

  // Windows SQL: prefix terms, injection-safe (quotes/wildcards stripped).
  {
    auto sql = os_search_windows_sql("firefox", 8);
    CHECK(sql.find("SELECT TOP 8") != std::string::npos);
    CHECK(sql.find("SystemIndex") != std::string::npos);
    CHECK(sql.find("\"firefox*\"") != std::string::npos);
  }
  {
    auto sql = os_search_windows_sql("quarterly report", 5);
    CHECK(sql.find("\"quarterly*\"") != std::string::npos);
    CHECK(sql.find("\"report*\"") != std::string::npos);
    CHECK(sql.find(" AND ") != std::string::npos);
  }
  {
    // Quote/wildcard metacharacters from the query never reach the SQL.
    auto sql = os_search_windows_sql("a'\"*; DROP", 8);
    CHECK(sql.find("\"a*\"") != std::string::npos);
    CHECK(sql.find("\"drop*\"") != std::string::npos);
    CHECK(sql.find(';') == std::string::npos);
    CHECK(sql.find("DROP") == std::string::npos);
    CHECK(sql.find('\'', sql.find("FileName")) != std::string::npos);  // CONTAINS quotes only
  }
  CHECK(os_search_windows_sql("", 8).empty());
  CHECK(os_search_windows_sql("   ", 8).empty());

  // Argv builders are shell-free (one tool + args, no /bin/sh).
  {
    auto a = os_search_argv("spotlight", "firefox", 8);
    CHECK_EQ(a.size(), 2u);
    CHECK_EQ(a[0], "mdfind");
    CHECK_EQ(a[1], "firefox");
  }
  {
    auto a = os_search_argv("tracker", "quarterly", 6);
    CHECK(!a.empty() && a[0] == "tracker3");
    CHECK_EQ(a.back(), "quarterly");
  }
  {
    auto a = os_search_argv("baloo", "notes", 8);
    CHECK(!a.empty() && a[0] == "baloosearch");
  }
  {
    auto a = os_search_argv("locate", "report.pdf", 8);
    CHECK(!a.empty() && a[0] == "locate");
    CHECK_EQ(a.back(), "report.pdf");
  }
  {
    auto a = os_search_argv("everything", "firefox", 8);
    CHECK(!a.empty());
  }
  {
    auto a = os_search_argv("windows_search", "firefox", 8);
    CHECK(!a.empty() && a[0] == "powershell");
    bool has_enc = false;
    for (auto& t : a)
      if (t == "-EncodedCommand") has_enc = true;
    CHECK(has_enc);
  }
  CHECK(os_search_argv("nope", "x", 8).empty());
  CHECK(os_search_argv("spotlight", "", 8).empty());

  // Parsers: headers/noise skipped, file:// URIs decoded.
  {
    auto hits = os_search_parse_output("spotlight",
                                       "/Users/a/Documents/report.pdf\n/Users/a/notes.txt\n", 8);
    CHECK_EQ(hits.size(), 2u);
    CHECK_EQ(hits[0], "/Users/a/Documents/report.pdf");
  }
  CHECK(os_search_parse_output("spotlight", "not a path\n...\n", 8).empty());
  {
    std::string out =
        "Results:\n"
        "  file:///home/u/Documents/quarterly%20report.pdf\n"
        "  file://localhost/home/u/notes.txt\n"
        "No pictures were found\n";
    auto hits = os_search_parse_output("tracker", out, 8);
    CHECK_EQ(hits.size(), 2u);
    CHECK_EQ(hits[0], "/home/u/Documents/quarterly report.pdf");
    CHECK_EQ(hits[1], "/home/u/notes.txt");
  }
  {
    auto hits = os_search_parse_output("tracker", "Results:\nNo results were found\n", 8);
    CHECK(hits.empty());
  }
  {
    auto hits = os_search_parse_output("baloo", "/home/u/a.txt\n/home/u/b.txt\n", 1);
    CHECK_EQ(hits.size(), 1u);
  }
  {
    auto hits = os_search_parse_output("windows_search",
                                       "C:\\Users\\a\\report.pdf\r\nC:\\tmp\\x.txt\r\n", 8);
    CHECK_EQ(hits.size(), 2u);
    CHECK_EQ(hits[0], "C:\\Users\\a\\report.pdf");
  }
  {
    // Noise from merged stderr must not become results.
    auto hits =
        os_search_parse_output("windows_search", "ERROR: something failed\nC:\\a\\b.txt\n", 8);
    CHECK_EQ(hits.size(), 1u);
  }

  // Result cards: deduped, labeled, capped, Open action.
  {
    auto cards = os_search_results_from_paths({"/a/report.pdf", "/a/report.pdf", "/b/notes.txt"},
                                              "spotlight", 8);
    CHECK_EQ(cards.size(), 2u);
    CHECK_EQ(cards[0].title, "report.pdf");
    CHECK(cards[0].subtitle.find("Spotlight") != std::string::npos);
    CHECK_EQ(cards[0].category, "os");
    CHECK(cards[0].score >= cards[1].score);
    CHECK(!cards[0].path.empty());
  }
  {
    auto cards = os_search_results_from_paths({"/a/1", "/a/2", "/a/3"}, "tracker", 2);
    CHECK_EQ(cards.size(), 2u);
  }

  // Provider honors config without touching the real OS index.
  {
    Config cfg;
    cfg.os_search.enabled = false;
    OsSearchProvider p;
    CHECK(p.query("firefox", cfg, 8).empty());
    CHECK_EQ(p.id(), "os");
  }
#ifdef _WIN32
  {
    // Native SystemIndex smoke: must not throw or crash. Hit counts depend
    // on the machine's index (CI runners may index nothing), so only the
    // contract is asserted: success implies bounded, path-shaped output.
    std::vector<std::string> paths;
    bool ok = false;
    try {
      ok = os_windows_search_com("firefox", 5, paths);
    } catch (...) {
      CHECK(false);
    }
    if (ok) {
      CHECK(paths.size() <= 5u);
      for (auto& p : paths)
        CHECK(!p.empty());
    }
    // Degenerate input never reaches COM.
    CHECK(!os_windows_search_com("", 5, paths));
    CHECK(!os_windows_search_com(std::string(300, 'x'), 5, paths));
  }
#endif
  {
    // Cross-OS backends are unsupported here and fail gracefully.
    Config cfg;
    cfg.os_search.enabled = true;
#if defined(_WIN32)
    cfg.os_search.backend = "spotlight";
#else
    cfg.os_search.backend = "windows_search";
#endif
    OsSearchProvider p;
    CHECK(p.query("firefox", cfg, 8).empty());
  }
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text("os_search:\n  enabled: true\n  backend: auto\n  max_results: 5\n", cfg,
                           err));
    CHECK(cfg.os_search.enabled);
    CHECK_EQ(cfg.os_search.backend, "auto");
    CHECK_EQ(cfg.os_search.max_results, 5);
  }
  {
    Config cfg;
    ConfigError err;
    CHECK(!load_config_text("os_search:\n  backend: nope\n", cfg, err));
    CHECK(!err.message.empty());
    CHECK(!load_config_text("os_search:\n  max_results: 99\n", cfg, err));
    CHECK(!load_config_text("os_search:\n  timeout_ms: 5\n", cfg, err));
    CHECK(!load_config_text("os_search:\n  bogus_key: 1\n", cfg, err));
  }
}
