// spoke_bench -- real throughput numbers, printed + saved to bench/results.txt.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "spoke/audio.h"
#include "spoke/config.h"
#include "spoke/features.h"
#include "spoke/model.h"
#include "spoke/threading.h"
#include "spoke/transcriber.h"

template <typename F>
static double median_ms(F&& fn, int iters = 5) {
  std::vector<double> ts;
  for (int i = 0; i < iters; ++i) {
    auto t0 = std::chrono::steady_clock::now();
    fn();
    auto t1 = std::chrono::steady_clock::now();
    ts.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
  }
  std::sort(ts.begin(), ts.end());
  return ts[ts.size() / 2];
}

int main() {
  std::cout << "spoke benchmark\n=================\n";
  spoke::SpokeConfig cfg = spoke::SpokeConfig::tiny();
  spoke::LogMelExtractor ext(cfg);
  spoke::AcousticModel model(cfg);
  spoke::ThreadPool pool(cfg.effectiveThreads());

  std::cout << "config: " << cfg.describe() << "\nparams: " << model.paramCount() << "\n";

  auto audio10 = spoke::generateNoisySpeechLike(10.0);
  auto feats10 = ext.extract(audio10);

  double feat_ms = median_ms([&] { (void)ext.extract(audio10); });
  double enc_ms = median_ms([&] { (void)model.forward(feats10, &pool); });

  std::cout << "frontend (10s) : " << feat_ms << " ms  (" << 10000.0 / feat_ms << "x realtime)\n";
  std::cout << "encoder  (10s) : " << enc_ms << " ms  (" << 10000.0 / enc_ms << "x realtime)\n";

  for (double secs : {1.0, 5.0, 10.0, 30.0}) {
    auto a = spoke::generateNoisySpeechLike(secs);
    spoke::Transcriber engine(cfg);
    spoke::TranscriptionResult r;
    auto t0 = std::chrono::steady_clock::now();
    (void)engine.transcribe(a, r);
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    double rtf = (ms / 1000.0) / secs;
    std::cout << "e2e " << secs << "s audio: " << ms << " ms  RTF=" << rtf << "\n";
  }

  // Persist machine-readable results for docs/benchmarks.md.
  std::ofstream f("results_bench.txt");
  if (f) {
    f << "frontend_10s_ms=" << feat_ms << "\nencoder_10s_ms=" << enc_ms << "\n";
    f << "params=" << model.paramCount() << "\nthreads=" << pool.size() << "\n";
  }
  std::cout << "done.\n";
  return 0;
}
