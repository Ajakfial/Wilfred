#include "wilfred/config/config.hpp"
#include "wilfred/index/record.hpp"
#include "wilfred/index/store.hpp"
#include "wilfred/index/string_pool.hpp"
#include "wilfred/index/tokenizer.hpp"
#include "wilfred/math/expr.hpp"
#include "wilfred/search/engine.hpp"
#include "wilfred/search/filter.hpp"
#include "wilfred/search/fuzzy.hpp"
#include "wilfred/search/rank.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace wilfred;
using namespace std::chrono;

struct BenchResult {
  std::string name;
  double min_ms;
  double max_ms;
  double mean_ms;
  double median_ms;
  double p95_ms;
};

std::vector<std::string> generate_synthetic_filenames(size_t count) {
  std::vector<std::string> names;
  names.reserve(count);
  const char* exts[] = {".cpp", ".hpp", ".pdf", ".png", ".txt", ".docx", ".exe", ".iso", ".zip"};
  const char* bases[] = {"project_abc_", "MyDocument_", "image_", "backup_", "data_", "test_"};
  for (size_t i = 0; i < count; ++i) {
    std::string n = bases[i % 6] + std::to_string(i) + exts[i % 9];
    names.push_back(n);
  }
  return names;
}

std::vector<std::string> generate_synthetic_paths(size_t count, const std::string& prefix = "/home/user/") {
  std::vector<std::string> paths;
  paths.reserve(count);
  const char* dirs[] = {"Documents", "Downloads", "Projects", "Pictures", "Music", "Videos",
                        "Code", "Work", "Personal", "Archive", "Temp", "Backups"};
  const char* exts[] = {".cpp", ".hpp", ".pdf", ".png", ".txt", ".docx", ".exe", ".iso", ".zip",
                        ".md", ".rs", ".go", ".js", ".ts", ".py", ".java", ".cs", ".rb", ".php"};
  const char* bases[] = {"project_", "document_", "image_", "backup_", "data_", "test_",
                         "script_", "config_", "log_", "note_", "report_", "archive_"};
  for (size_t i = 0; i < count; ++i) {
    std::string p = prefix + dirs[i % 12] + "/" + bases[i % 12] + std::to_string(i) + exts[i % 20];
    paths.push_back(p);
  }
  return paths;
}

BenchResult run_benchmark(const std::string& name, size_t iterations, std::function<void()> fn) {
  std::vector<double> times;
  times.reserve(iterations);

  // warmup
  fn();

  for (size_t i = 0; i < iterations; ++i) {
    auto start = high_resolution_clock::now();
    fn();
    auto end = high_resolution_clock::now();
    double ms = duration<double, std::milli>(end - start).count();
    times.push_back(ms);
  }

  std::sort(times.begin(), times.end());
  double sum = 0;
  for (auto t : times)
    sum += t;

  BenchResult res;
  res.name = name;
  res.min_ms = times.front();
  res.max_ms = times.back();
  res.mean_ms = sum / times.size();
  res.median_ms = times[times.size() / 2];
  res.p95_ms = times[size_t(times.size() * 0.95)];
  return res;
}

void print_results(const std::vector<BenchResult>& results) {
  std::cout << std::left << std::setw(35) << "Benchmark" << std::right << std::setw(12)
            << "Min (ms)" << std::setw(12) << "Median (ms)" << std::setw(12) << "Mean (ms)"
            << std::setw(12) << "P95 (ms)" << std::setw(12) << "Max (ms)\n";
  std::cout << std::string(95, '-') << "\n";

  for (const auto& r : results) {
    std::cout << std::left << std::setw(35) << r.name << std::right << std::fixed
              << std::setprecision(3) << std::setw(12) << r.min_ms << std::setw(12) << r.median_ms
              << std::setw(12) << r.mean_ms << std::setw(12) << r.p95_ms << std::setw(12)
              << r.max_ms << "\n";
  }
}

int main() {
  std::vector<BenchResult> results;
  int iters = 10;

  auto filenames_100k = generate_synthetic_filenames(100000);
  auto filenames_10k = generate_synthetic_filenames(10000);
  auto filenames_1k = generate_synthetic_filenames(1000);
  auto paths_100k = generate_synthetic_paths(100000);
  auto paths_10k = generate_synthetic_paths(10000);

  // String pool
  results.push_back(run_benchmark("String pool: intern 100K", iters, [&]() {
    StringPool pool;
    for (const auto& name : filenames_100k) {
      pool.intern(name);
    }
  }));
  results.push_back(run_benchmark("String pool: intern 1M", 3, [&]() {
    StringPool pool;
    auto fnames_1m = generate_synthetic_filenames(1000000);
    for (const auto& name : fnames_1m) {
      pool.intern(name);
    }
  }));

  // Tokenizer
  results.push_back(run_benchmark("Tokenizer: tokenize 100K", iters, [&]() {
    for (const auto& name : filenames_100k) {
      auto t = tokenize_name(name);
    }
  }));
  results.push_back(run_benchmark("Tokenizer: tokenize 1M", 3, [&]() {
    auto fnames_1m = generate_synthetic_filenames(1000000);
    for (const auto& name : fnames_1m) {
      auto t = tokenize_name(name);
    }
  }));

  // Index insert
  auto insert_records = [&](IndexStore& store, size_t count,
                            const std::vector<std::string>& fnames,
                            const std::vector<std::string>& pnames) {
    store.clear();
    for (size_t i = 0; i < count; ++i) {
      IndexRecord rec;
      rec.name_id = store.pool().intern(fnames[i]);
      rec.path_id = store.pool().intern(pnames[i]);
      store.upsert(rec, store.pool().get(rec.path_id));
    }
  };

  results.push_back(run_benchmark("Index insert: 1K", iters, [&]() {
    IndexStore store;
    insert_records(store, 1000, filenames_1k, paths_10k);
  }));
  results.push_back(run_benchmark("Index insert: 10K", iters, [&]() {
    IndexStore store;
    insert_records(store, 10000, filenames_10k, paths_10k);
  }));
  results.push_back(run_benchmark("Index insert: 100K", 5, [&]() {
    IndexStore store;
    insert_records(store, 100000, filenames_100k, paths_100k);
  }));
  results.push_back(run_benchmark("Index insert: 1M", 2, [&]() {
    IndexStore store;
    auto fnames_1m = generate_synthetic_filenames(1000000);
    auto pnames_1m = generate_synthetic_paths(1000000);
    insert_records(store, 1000000, fnames_1m, pnames_1m);
  }));

  // Index query
  IndexStore store_10k;
  insert_records(store_10k, 10000, filenames_10k, paths_10k);
  IndexStore store_100k;
  insert_records(store_100k, 100000, filenames_100k, paths_100k);
  IndexStore store_1m;
  {
    auto fnames_1m = generate_synthetic_filenames(1000000);
    auto pnames_1m = generate_synthetic_paths(1000000);
    insert_records(store_1m, 1000000, fnames_1m, pnames_1m);
  }

  results.push_back(run_benchmark("Index query: 10K records", iters, [&]() {
    auto pt = store_10k.pool().find("project");
    if (pt != StringPool::kInvalid) store_10k.posting(pt);
  }));

  results.push_back(run_benchmark("Index query: 100K records", iters, [&]() {
    auto pt = store_100k.pool().find("project");
    if (pt != StringPool::kInvalid) store_100k.posting(pt);
  }));

  results.push_back(run_benchmark("Index query: 1M records", 5, [&]() {
    auto pt = store_1m.pool().find("project");
    if (pt != StringPool::kInvalid) store_1m.posting(pt);
  }));

  // Fuzzy search
  results.push_back(run_benchmark("Fuzzy search: 10K targets", iters, [&]() {
    for (size_t i = 0; i < 10000; ++i) {
      score_fuzzy("proj", filenames_10k[i], filenames_10k[i]);
    }
  }));
  results.push_back(run_benchmark("Fuzzy search: 100K targets", 5, [&]() {
    for (size_t i = 0; i < 100000; ++i) {
      score_fuzzy("proj", filenames_100k[i], filenames_100k[i]);
    }
  }));

  // Ranking
  std::vector<ScoredHit> hits_10k;
  for (size_t i = 0; i < 10000; ++i) {
    hits_10k.push_back({static_cast<std::uint32_t>(i), 50, store_10k.get(static_cast<std::uint32_t>(i + 1))});
  }
  std::vector<ScoredHit> hits_100k;
  for (size_t i = 0; i < 100000; ++i) {
    hits_100k.push_back({static_cast<std::uint32_t>(i), 50, store_100k.get(static_cast<std::uint32_t>(i + 1))});
  }
  std::vector<ScoredHit> hits_1m;
  for (size_t i = 0; i < 1000000; ++i) {
    hits_1m.push_back({static_cast<std::uint32_t>(i), 50, store_1m.get(static_cast<std::uint32_t>(i + 1))});
  }

  results.push_back(run_benchmark("Ranking: 10K scored hits", iters, [&]() {
    RankContext ctx;
    ctx.query = "proj";
    RankingWeights w;
    for (auto& h : hits_10k) {
      if (h.rec) h.score = rank_record(ctx, store_10k, *h.rec, w);
    }
    take_top(hits_10k, 40);
  }));
  results.push_back(run_benchmark("Ranking: 100K scored hits", 5, [&]() {
    RankContext ctx;
    ctx.query = "proj";
    RankingWeights w;
    for (auto& h : hits_100k) {
      if (h.rec) h.score = rank_record(ctx, store_100k, *h.rec, w);
    }
    take_top(hits_100k, 40);
  }));
  results.push_back(run_benchmark("Ranking: 1M scored hits", 2, [&]() {
    RankContext ctx;
    ctx.query = "proj";
    RankingWeights w;
    for (auto& h : hits_1m) {
      if (h.rec) h.score = rank_record(ctx, store_1m, *h.rec, w);
    }
    take_top(hits_1m, 40);
  }));

  // Filter chain
  SearchFilter f;
  f.extensions = {".cpp", ".hpp"};
  results.push_back(run_benchmark("Filter chain: 10K records", iters, [&]() {
    size_t match_count = 0;
    for (size_t i = 0; i < 10000; ++i) {
      if (auto rec = store_10k.get(static_cast<std::uint32_t>(i + 1))) {
        if (record_matches_filter(f, store_10k, *rec)) match_count++;
      }
    }
  }));
  results.push_back(run_benchmark("Filter chain: 100K records", 5, [&]() {
    size_t match_count = 0;
    for (size_t i = 0; i < 100000; ++i) {
      if (auto rec = store_100k.get(static_cast<std::uint32_t>(i + 1))) {
        if (record_matches_filter(f, store_100k, *rec)) match_count++;
      }
    }
  }));
  results.push_back(run_benchmark("Filter chain: 1M records", 2, [&]() {
    size_t match_count = 0;
    for (size_t i = 0; i < 1000000; ++i) {
      if (auto rec = store_1m.get(static_cast<std::uint32_t>(i + 1))) {
        if (record_matches_filter(f, store_1m, *rec)) match_count++;
      }
    }
  }));

  // Math evaluation
  results.push_back(run_benchmark("Math evaluation: 1K expressions", iters, [&]() {
    for (int i = 0; i < 1000; ++i) {
      evaluate_math("123 * 456 + (789 / 2)");
    }
  }));

  // Snapshot save/load
  store_10k.save("bench_10k.snapshot");
  results.push_back(run_benchmark("Snapshot load: 10K", iters, [&]() {
    IndexStore loaded;
    loaded.load("bench_10k.snapshot");
  }));
  results.push_back(
      run_benchmark("Snapshot save: 10K", iters, [&]() { store_10k.save("bench_10k.snapshot"); }));
  store_100k.save("bench_100k.snapshot");
  results.push_back(run_benchmark("Snapshot load: 100K", 5, [&]() {
    IndexStore loaded;
    loaded.load("bench_100k.snapshot");
  }));
  results.push_back(run_benchmark("Snapshot save: 100K", 5, [&]() { store_100k.save("bench_100k.snapshot"); }));
  store_1m.save("bench_1m.snapshot");
  results.push_back(run_benchmark("Snapshot load: 1M", 2, [&]() {
    IndexStore loaded;
    loaded.load("bench_1m.snapshot");
  }));
  results.push_back(run_benchmark("Snapshot save: 1M", 2, [&]() { store_1m.save("bench_1m.snapshot"); }));

  // Memory reporting
  results.push_back(run_benchmark("Memory reporting: 100K index", iters, [&]() {
    size_t pool_bytes = store_100k.pool().bytes();
    size_t rec_bytes = store_100k.records().size() * sizeof(IndexRecord);
    volatile size_t total = pool_bytes + rec_bytes;
    (void)total;
  }));
  results.push_back(run_benchmark("Memory reporting: 1M index", 3, [&]() {
    size_t pool_bytes = store_1m.pool().bytes();
    size_t rec_bytes = store_1m.records().size() * sizeof(IndexRecord);
    volatile size_t total = pool_bytes + rec_bytes;
    (void)total;
  }));

  print_results(results);
  return 0;
}
