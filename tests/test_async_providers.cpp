#include "test.hpp"
#include "wilfred/config/config.hpp"
#include "wilfred/index/engine.hpp"
#include "wilfred/providers/provider.hpp"
#include "wilfred/query/interpreter.hpp"
#include "wilfred/search/async_providers.hpp"
#include "wilfred/search/engine.hpp"

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace {

using namespace wilfred;

SearchResult card(const std::string& title, const std::string& path, const std::string& cat,
                  int score = 400) {
  SearchResult r;
  r.title = title;
  r.subtitle = "test";
  r.path = path;
  r.payload = path;
  r.action = ResultAction::Open;
  r.score = score;
  r.kind_label = cat;
  r.category = cat;
  return r;
}

struct StubProvider : SearchProvider {
  std::string id() const override { return "stub"; }
  std::vector<SearchResult> query(const std::string&, const Config&, std::size_t) override {
    return {card("stub-marker", "stub:path", "stub", 10)};
  }
};

template <typename Pred>
bool wait_for(Pred p, int timeout_ms = 5000) {
  auto start = std::chrono::steady_clock::now();
  while (!p()) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() -
                                                              start)
            .count() > timeout_ms)
      return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  return true;
}

}  // namespace

void test_async_providers() {
  using namespace wilfred;
  // --- merge: os dedup, exact-repeat collapse, passthrough ---
  {
    std::vector<SearchResult> base = {card("a.txt", "/x/a.txt", "file", 900)};
    std::vector<SearchResult> extra = {
        card("a.txt", "/x/a.txt", "os"),                   // dup of base
        card("a.txt", "/X/A.TXT", "os"),                   // case-insensitive dup
        card("b.txt", "C:\\y\\b.txt", "os"),               // new
        card("b.txt", "C:/y/b.txt", "os"),                 // separator-insensitive dup
        card("web hit", "https://example.com", "remote"),  // passthrough
        card("web hit", "https://example.com", "remote"),  // exact repeat collapses
        card("other", "https://example.com", "remote"),    // same path, other card passes
    };
    merge_provider_results(base, std::move(extra));
    CHECK_EQ(base.size(), 4u);  // a.txt + b.txt + web hit + other
    CHECK_EQ(base[1].title, "b.txt");
    CHECK_EQ(base[2].category, "remote");
  }
  {
    std::vector<SearchResult> base = {card("a", "/a", "file", 1)};
    std::vector<SearchResult> empty;
    merge_provider_results(base, std::move(empty));
    CHECK_EQ(base.size(), 1u);
  }

  // --- interpret_fast: full pipeline minus providers ---
  {
    Config cfg;
    IndexEngine index;
    SearchEngine search(index);
    QueryInterpreter interp(index, search);
    interp.providers().add(std::make_unique<StubProvider>());
    auto full = interp.interpret("zzqxjv-kittens", cfg, nullptr);
    auto fast = interp.interpret_fast("zzqxjv-kittens", cfg, nullptr);
    bool full_has_stub = false;
    for (auto& r : full.results)
      if (r.title == "stub-marker") full_has_stub = true;
    CHECK(full_has_stub);
    for (auto& r : fast.iq.results)
      CHECK(r.title != "stub-marker");
    CHECK(fast.providers_apply);
    CHECK_EQ(fast.effective, "zzqxjv-kittens");
    // Everything else identical: fast + marker == full.
    auto merged = fast.iq.results;
    merge_provider_results(merged, interp.query_providers("zzqxjv-kittens", cfg, 40));
    CHECK_EQ(merged.size(), full.results.size());
    for (std::size_t i = 0; i < merged.size() && i < full.results.size(); ++i)
      CHECK_EQ(merged[i].title, full.results[i].title);
    // Early-return paths never reach providers (no calculator + file mix).
    auto math = interp.interpret_fast("25 * 42", cfg, nullptr);
    CHECK(!math.iq.results.empty());
    CHECK(!math.providers_apply);
    auto url = interp.interpret_fast("https://example.com", cfg, nullptr);
    CHECK(!url.providers_apply);
    // Aliases expand for the provider stage.
    cfg.aliases["ed"] = "Visual Studio Code";
    auto aliased = interp.interpret_fast("ed", cfg, nullptr);
    CHECK_EQ(aliased.effective, "Visual Studio Code");
  }

  // --- latest-wins: stale generation never pushes ---
  {
    AsyncProviders ap;
    std::atomic<bool> release{false};
    std::atomic<int> started{0};
    std::mutex mu;
    std::vector<std::uint64_t> pushed;
    auto push = [&](std::uint64_t g, std::vector<SearchResult>) {
      std::lock_guard<std::mutex> l(mu);
      pushed.push_back(g);
    };
    ap.request(
        1, "qa-blocking", 8,
        [&] {
          ++started;
          while (!release.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          return std::vector<SearchResult>{card("A", "/a", "stub")};
        },
        push);
    CHECK(wait_for([&] { return started.load() >= 1; }));
    ap.request(
        2, "qb-quick", 8, [&] { return std::vector<SearchResult>{card("B", "/b", "stub")}; }, push);
    release = true;
    CHECK(wait_for([&] {
      std::lock_guard<std::mutex> l(mu);
      return pushed.size() >= 1;
    }));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // let any strays land
    std::lock_guard<std::mutex> l(mu);
    CHECK_EQ(pushed.size(), 1u);
    if (!pushed.empty()) CHECK_EQ(pushed[0], 2u);
  }

  // --- cache: repeat queries skip the run; limit and ttl respected ---
  {
    AsyncProviders ap;
    std::atomic<int> runs{0};
    std::mutex mu;
    std::vector<std::vector<SearchResult>> pushes;
    auto push = [&](std::uint64_t, std::vector<SearchResult> r) {
      std::lock_guard<std::mutex> l(mu);
      pushes.push_back(std::move(r));
    };
    auto run = [&] {
      ++runs;
      return std::vector<SearchResult>{card("C", "/c", "stub")};
    };
    ap.request(1, "qc", 8, run, push);
    CHECK(wait_for([&] {
      std::lock_guard<std::mutex> l(mu);
      return pushes.size() >= 1;
    }));
    CHECK_EQ(runs.load(), 1);
    std::vector<SearchResult> hit;
    CHECK(ap.cached("qc", 8, hit));
    CHECK_EQ(hit.size(), 1u);
    CHECK(!ap.cached("nope", 8, hit));
    CHECK(!ap.cached("qc", 50, hit));  // entry covers 8, not 50
    // Repeat: served from cache, run not invoked again.
    ap.request(2, "qc", 8, run, push);
    CHECK(wait_for([&] {
      std::lock_guard<std::mutex> l(mu);
      return pushes.size() >= 2;
    }));
    CHECK_EQ(runs.load(), 1);
    // Larger limit: cache miss, reruns.
    ap.request(3, "qc", 50, run, push);
    CHECK(wait_for([&] { return runs.load() >= 2; }));
  }
  {
    // Zero TTL: everything is stale, every request reruns.
    AsyncProviders ap(std::chrono::seconds(0));
    std::atomic<int> runs{0};
    std::mutex mu;
    int pushes = 0;
    auto push = [&](std::uint64_t, std::vector<SearchResult>) {
      std::lock_guard<std::mutex> l(mu);
      ++pushes;
    };
    auto run = [&] {
      ++runs;
      return std::vector<SearchResult>{};
    };
    ap.request(1, "qd", 8, run, push);
    CHECK(wait_for([&] { return runs.load() >= 1; }));
    std::vector<SearchResult> hit;
    CHECK(!ap.cached("qd", 8, hit));
    ap.request(2, "qd", 8, run, push);
    CHECK(wait_for([&] { return runs.load() >= 2; }));
  }

  // --- registry stats: per-provider calls, timing, hits ---
  {
    ProviderRegistry reg;
    struct Slow : SearchProvider {
      std::string id() const override { return "slow"; }
      std::vector<SearchResult> query(const std::string&, const Config&,
                                      std::size_t) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        return {card("s", "/s", "stub")};
      }
    };
    struct Boom : SearchProvider {
      std::string id() const override { return "boom"; }
      std::vector<SearchResult> query(const std::string&, const Config&,
                                      std::size_t) override {
        throw 1;
      }
    };
    reg.add(std::make_unique<Slow>());
    reg.add(std::make_unique<Boom>());
    Config cfg;
    auto hits = reg.query_all("firefox", cfg, 8);
    CHECK_EQ(hits.size(), 1u);  // throwing provider contributes nothing, nothing propagates
    auto st = reg.stats();
    CHECK_EQ(st.size(), 2u);
    CHECK_EQ(st["slow"].calls, 1u);
    CHECK_EQ(st["slow"].last_hits, 1u);
    CHECK(st["slow"].total_us >= 20000u);  // sleep guarantees the floor
    CHECK_EQ(st["boom"].calls, 1u);
    CHECK_EQ(st["boom"].last_hits, 0u);
    reg.query_all("firefox", cfg, 8);
    CHECK_EQ(reg.stats()["slow"].calls, 2u);
    reg.reset_stats();
    CHECK(reg.stats().empty());
  }
}
