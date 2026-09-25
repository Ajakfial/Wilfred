#include "test.hpp"
#include "wilfred/config/config.hpp"

void test_config() {
  const char* text = R"(
search:
  include_system_files: false
  max_results: 12
  fuzzy: true
index:
  paths:
    - "C:/Users"
    - "D:/Projects"
  exclude:
    - node_modules
    - .git
  extensions:
    include:
      - .cpp
      - .h
    exclude:
      - .tmp
  cpu_percent_limit: 40
  memory_limit_mb: 128
aliases:
  code: Visual Studio Code
custom_metadata:
  owner: whistlest
browser:
  search_template: "https://example.com/q={query}"
)";
  wilfred::Config cfg;
  wilfred::ConfigError err;
  CHECK(load_config_text(text, cfg, err));
  CHECK_EQ(cfg.search.max_results, 12);
  CHECK_EQ(cfg.index.paths.size(), 2u);
  CHECK(path_is_excluded(cfg, "D:/Projects/foo/node_modules/x", "node_modules"));
  CHECK(!extension_allowed(cfg, ".tmp"));
  CHECK(extension_allowed(cfg, ".cpp"));
  CHECK_EQ(cfg.aliases["code"], "Visual Studio Code");
  CHECK_EQ(cfg.custom_metadata["owner"], "whistlest");

  wilfred::Config bad;
  CHECK(!load_config_text("search:\n  max_results: 9999\n", bad, err));
  CHECK(!err.message.empty());

  CHECK(!load_config_text("index:\n  cpu_percent_limit: 1\n", bad, err));
  CHECK(!load_config_text("logging:\n  level: verbose\n", bad, err));
  CHECK(!load_config_text("browser:\n  search_template: https://x.com\n", bad, err));
  CHECK(!load_config_text("not yaml: [\n", bad, err));
}
