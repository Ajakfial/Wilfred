#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/plugin/host.hpp"
#include "wilfred/search/actions.hpp"
#include "wilfred/search/snippets.hpp"
#include "wilfred/sync/backup.hpp"

void test_extensibility() {
  using namespace wilfred;

  std::string name;
  CHECK(query_is_snippet_save("snip save sig", name));
  CHECK_EQ(name, "sig");
  CHECK(!query_is_snippet_save("snip sig", name));

  Config cfg;
  CHECK(snippet_query_forced(";hello", cfg));
  CHECK(snippet_query_forced("snip foo", cfg));
  CHECK(!snippet_query_forced("firefox", cfg));

  SnippetStore store;
  Snippet s;
  s.id = "sig";
  s.trigger = "sig";
  s.title = "Signature";
  s.body = "Thanks,\nJay";
  store.upsert(s);
  auto hits = store.match(";sig", cfg);
  CHECK(!hits.empty());
  CHECK_EQ(hits.front().action, ResultAction::Expand);
  CHECK_EQ(hits.front().payload, "Thanks,\nJay");

  SearchResult file;
  file.title = "notes.md";
  file.path = "C:/tmp/notes.md";
  file.action = ResultAction::Open;
  attach_result_actions(file);
  CHECK(file.actions.size() >= 3);
  CHECK_EQ(file.actions[0].id, "open");
  CHECK(action_hides_overlay("open"));
  CHECK(!action_hides_overlay("copy_path"));

  auto json = std::string("{\"results\":[{\"title\":\"Ping\",\"path\":\"x\",\"score\":42,\"action\":\"open\"}]}");
  auto plug = parse_plugin_results_json(json, "demo");
  CHECK_EQ(plug.size(), 1u);
  CHECK_EQ(plug[0].plugin_id, "demo");
  CHECK_EQ(plug[0].category, "plugin");
  CHECK_EQ(plug[0].action, ResultAction::Plugin);

  std::vector<BackupEntry> files;
  files.push_back({"wilfred.yml", "search:\n  max_results: 8\n"});
  std::string blob, err;
  CHECK(pack_backup_archive(files, blob, err));
  std::vector<BackupEntry> out;
  CHECK(unpack_backup_archive(blob, out, err));
  CHECK_EQ(out.size(), 1u);
  CHECK_EQ(out[0].name, "wilfred.yml");

  Config loaded;
  ConfigError cerr;
  CHECK(load_config_text(R"(
plugins:
  enabled: true
  timeout_ms: 250
api:
  enabled: true
  port: 18000
sync:
  url: "http://127.0.0.1:18000/backup"
snippets:
  prefix: ";"
  items:
    sig: "Best regards"
)",
                         loaded, cerr));
  CHECK(loaded.plugins.enabled);
  CHECK_EQ(loaded.plugins.timeout_ms, 250);
  CHECK(loaded.api.enabled);
  CHECK_EQ(loaded.api.port, 18000);
  CHECK_EQ(loaded.snippets.items["sig"], "Best regards");
}
