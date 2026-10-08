#include "test.hpp"

#include "wilfred/config/config.hpp"
#include "wilfred/core/mmap.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/plugin/registry.hpp"
#include "wilfred/search/embedding.hpp"
#include "wilfred/search/semantic.hpp"

#include <filesystem>

void test_plugins() {
  using namespace wilfred;
  namespace fs = std::filesystem;

  // --- Registry per-OS artifacts: parse + resolve ---
  {
    auto entries = plugin_registry_parse(
        R"({"plugins":[
          {"id":"dice","version":"1.0.0","kind":"native","description":"d",
           "permissions":[],
           "artifacts":{
             "windows":{"url":"https://x/dice-win.zip","sha256":"aa"},
             "macos":{"url":"https://x/dice-mac.zip","sha256":"bb"},
             "linux":{"url":"https://x/dice-lin.zip","sha256":"cc"}}},
          {"id":"legacy","version":"0.1","kind":"stdio",
           "url":"https://x/legacy.sh","sha256":"dd"},
          {"id":"nowhere","version":"0.1"},
          {"id":"","version":"0.1","url":"https://x/e"}
        ]})");
    CHECK_EQ(entries.size(), 2u);  // nowhere (no url/artifacts) + empty id dropped
    const RegistryEntry* dice = nullptr;
    const RegistryEntry* legacy = nullptr;
    for (auto& e : entries) {
      if (e.id == "dice") dice = &e;
      if (e.id == "legacy") legacy = &e;
    }
    CHECK(dice != nullptr && legacy != nullptr);
    CHECK_EQ(dice->artifacts.size(), 3u);
    auto os = plugin_registry_artifact_os();
    CHECK(os == "windows" || os == "macos" || os == "linux");
    std::string url, sha;
    CHECK(plugin_registry_resolve(*dice, url, sha));
    CHECK(url == "https://x/dice-" + (os == "windows" ? std::string("win")
                                      : os == "macos" ? std::string("mac")
                                                      : std::string("lin")) + ".zip");
    CHECK(plugin_registry_resolve(*legacy, url, sha));
    CHECK_EQ(url, "https://x/legacy.sh");
    CHECK_EQ(sha, "dd");
    RegistryEntry empty;
    empty.id = "empty";
    CHECK(!plugin_registry_resolve(empty, url, sha));
    CHECK(plugin_registry_parse("garbage").empty());
  }

  // --- Gallery sources: manifest + README + query-protocol smoke shape ---
  {
#ifdef WILFRED_SOURCE_DIR
    std::string root = WILFRED_SOURCE_DIR;
    int count = 0;
    for (auto& id : {"dice", "morse", "password", "cheatsheets"}) {
      auto dir = fs::u8path(root + "/plugins/" + id);
      CHECK(fs::exists(dir / "plugin.yml"));
      CHECK(fs::exists(dir / "README.md"));
      bool has_c = false;
      std::error_code ec;
      for (auto it = fs::directory_iterator(dir, ec); it != fs::directory_iterator();
           it.increment(ec)) {
        if (ec) break;
        auto ext = it->path().extension().string();
        if (ext == ".c" || ext == ".cpp") has_c = true;
      }
      CHECK(has_c);
      ++count;
    }
    CHECK_EQ(count, 4);
    // Registry skeleton lists every flagship (artifacts filled by CI pack).
    std::string text;
    CHECK(read_file_all(root + "/plugins/registry.json", text));
    for (auto& id : {"dice", "morse", "password", "cheatsheets"})
      CHECK(text.find(id) != std::string::npos);
#else
    CHECK(false);
#endif
  }

  // --- Semantic fusion weights: config + scoring effect ---
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text("providers:\n  semantic: true\n", cfg, err));
    CHECK_EQ(cfg.providers.semantic_vector_weight, 1.0);
    CHECK_EQ(cfg.providers.semantic_trigram_weight, 1.0);
    CHECK(load_config_text(
        "providers:\n  semantic: true\n  semantic_vector_weight: 0\n  semantic_trigram_weight: 2\n",
        cfg, err));
    CHECK_EQ(cfg.providers.semantic_vector_weight, 0.0);
    CHECK_EQ(cfg.providers.semantic_trigram_weight, 2.0);
    Config bad;
    CHECK(!load_config_text("providers:\n  semantic_trigram_weight: 9\n", bad, err));
    CHECK(!load_config_text("providers:\n  semantic_vector_weight: -1\n", bad, err));
  }
  {
    IndexEngine index;
    IndexRecord r1;
    r1.kind = FileKind::Application;
    index.store().upsert(r1, "/apps/Visual Studio Code");
    IndexRecord r2;
    r2.kind = FileKind::Application;
    index.store().upsert(r2, "/apps/Firefox");
    SemanticProvider prov(index);
    Config flat;
    flat.providers.semantic = true;
    flat.providers.semantic_min_score = 0.2;
    flat.providers.semantic_trigram_weight = 0.0;  // mutes the only active backend
    auto muted = prov.query("visual studio", flat, 10);
    CHECK(!muted.empty());
    for (auto& h : muted) CHECK_EQ(h.score, 500);
    Config loud;
    loud.providers.semantic = true;
    loud.providers.semantic_min_score = 0.2;
    loud.providers.semantic_trigram_weight = 2.0;
    auto boosted = prov.query("visual studio", loud, 10);
    CHECK(!boosted.empty());
    CHECK(boosted.front().score > 500);
    CHECK(boosted.front().title.find("Visual Studio") != std::string::npos);
  }

  // --- Hash embedder properties ---
  {
    auto a = hash_embed_text("visual studio code", 128);
    auto b = hash_embed_text("visual studio code", 128);
    CHECK_EQ(a.size(), 128u);
    for (std::size_t i = 0; i < a.size(); ++i) CHECK_EQ(a[i], b[i]);  // deterministic
    CHECK_NEAR(embedding_cosine(a, a), 1.0, 1e-5);
    auto rel = hash_embed_text("visual studio code editor", 128);
    auto far = hash_embed_text("zebra xylophone quantum", 128);
    CHECK(embedding_cosine(a, a) >= embedding_cosine(a, rel));
    CHECK(embedding_cosine(a, rel) > embedding_cosine(a, far));
    CHECK_NEAR(embedding_cosine(a, b), embedding_cosine(b, a), 1e-9);  // symmetric
    // Repetition dampening keeps degenerate inputs sane (self is still 1.0,
    // and repeats don't collapse onto unrelated texts).
    auto rep = hash_embed_text("code code code code code", 128);
    CHECK_NEAR(embedding_cosine(rep, rep), 1.0, 1e-5);
    CHECK(embedding_cosine(rep, a) > embedding_cosine(rep, far));
  }
}
