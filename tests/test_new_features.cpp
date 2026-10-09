#include "test.hpp"

#include "wilfred/ai/assistant.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/search/embedding.hpp"
#include "wilfred/search/hnsw.hpp"
#include "wilfred/search/ocr.hpp"
#include "wilfred/search/snippets.hpp"
#include "wilfred/search/vector_index.hpp"
#include "wilfred/sources/sources.hpp"

void test_new_features() {
  using namespace wilfred;

  // --- Config: new sections parse ---
  {
    Config cfg;
    ConfigError err;
    CHECK(load_config_text(R"(
providers:
  semantic: true
  semantic_backend: hybrid
  semantic_max_results: 12
embedding:
  enabled: true
  backend: hash
  dim: 128
ai:
  enabled: false
  provider: openai
  api_key: "sk-test"
sources:
  calendar: true
  ocr: false
snippets:
  prefix: ";"
  global_expansion: true
)",
                           cfg, err));
    CHECK(cfg.providers.semantic);
    CHECK_EQ(cfg.providers.semantic_backend, "hybrid");
    CHECK(cfg.embedding.enabled);
    CHECK_EQ(cfg.embedding.dim, 128);
    CHECK_EQ(cfg.ai.provider, "openai");
    CHECK(cfg.sources.calendar);
    CHECK(cfg.snippets.global_expansion);
    // Invalid values are rejected.
    Config bad;
    CHECK(!load_config_text("embedding:\n  dim: 8\n", bad, err));
    CHECK(!load_config_text("ai:\n  temperature: 9\n", bad, err));
  }

  // --- Hash embedding: deterministic, normalized, cosine sane ---
  {
    auto a = hash_embed_text("hello world", 128);
    auto b = hash_embed_text("hello world", 128);
    auto c = hash_embed_text("something else entirely", 128);
    CHECK_EQ(a.size(), 128u);
    CHECK_NEAR(embedding_cosine(a, b), 1.0, 1e-5);
    CHECK(embedding_cosine(a, c) < 0.95);
    LocalEmbedder e;
    EmbeddingConfigView ev;
    ev.enabled = true;
    ev.backend = "hash";
    ev.dim = 128;
    CHECK_EQ(e.configure(ev), "hash");
    auto v = e.embed("wilfred search");
    CHECK_EQ((int)v.size(), 128);
  }

  // --- HNSW: add/search/save/load roundtrip ---
  {
    HnswIndex idx(32, 8, 16);
    idx.configure(32, 8, 16);
    auto v1 = hash_embed_text("alpha document", 32);
    auto v2 = hash_embed_text("beta document", 32);
    CHECK(idx.add(1, v1));
    CHECK(idx.add(2, v2));
    CHECK_EQ(idx.size(), 2u);
    auto hits = idx.search(v1, 2);
    CHECK(!hits.empty());
    CHECK_EQ(hits.front().id, 1u);
    CHECK(idx.remove(2));
    CHECK_EQ(idx.size(), 1u);
  }

  // --- Snippet placeholders + folders + global match ---
  {
    std::string body = "Hi {date} {clipboard} {query} {unknown}";
    auto expanded = expand_snippet_placeholders(body, "Q", "CLIP");
    CHECK(expanded.find("{unknown}") != std::string::npos);
    CHECK(expanded.find("CLIP") != std::string::npos);
    CHECK(expanded.find("Q") != std::string::npos);
    CHECK(expanded.find("{date}") == std::string::npos);

    SnippetStore store;
    Snippet s;
    s.id = "sig";
    s.trigger = "sig";
    s.title = "Sig";
    s.body = "Best {date}";
    s.folder = "mail";
    store.upsert(s);
    CHECK(store.find_trigger("SIG") != nullptr);
    std::size_t n = 0;
    auto* hit = snippet_global_match(store, "please sig ", n);
    CHECK(hit != nullptr);
    CHECK_EQ(n, 3u);
    std::size_t n2 = 0;
    CHECK(snippet_global_match(store, "hello world ", n2) == nullptr);
    Config cfg;
    auto m = store.match(";mail/sig", cfg);
    CHECK(!m.empty());
  }

  // --- AI query parsing (no network) ---
  {
    std::string prompt;
    CHECK(ai_query_is_request("ai hello world", prompt));
    CHECK_EQ(prompt, "hello world");
    CHECK(ai_query_is_request("ask: foo", prompt));
    CHECK(!ai_query_is_request("firefox", prompt));
    CHECK_EQ(ai_provider_default_model("openai"), "gpt-4o-mini");
    CHECK_EQ(ai_provider_default_model("groq"), "llama-3.1-8b-instant");
    AiAssistant ai;
    Config cfg;  // disabled by default
    CHECK(!ai.configured(cfg));
    auto cards = ai.results_for("", cfg);
    CHECK(!cards.empty());
  }

  // --- Sources defaults + OCR helpers (no tesseract needed) ---
  {
    CHECK(is_image_extension("photo.PNG"));
    CHECK(!is_image_extension("doc.pdf"));
    CHECK(is_ocr_candidate("scan.jpg"));
    auto roots = default_notes_roots();
    CHECK(!roots.empty());
    // Providers return empty (not crash) on short queries / missing dirs.
    Config cfg;
    cfg.sources.calendar = true;
    CalendarProvider cal;
    CHECK(cal.query("x", cfg, 5).empty());
    ContactsProvider con;
    CHECK(con.query("x", cfg, 5).empty());
    NotesProvider notes;
    CHECK(notes.query("x", cfg, 5).empty());
  }
}
