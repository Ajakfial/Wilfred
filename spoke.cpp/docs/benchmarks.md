# Benchmarks -- real numbers, reproducible

Methodology: `bench/bench_main.cpp` synthesizes deterministic speech-like
audio (harmonic complex + 3 Hz syllabic AM + noise, seed 1234), runs each
stage 5×, and reports the **median**. End-to-end covers VAD → frontend →
encoder → CTC decode including all allocations. Machine: 6-thread laptop
CPU, Release build, tiny config (310,813 params, int8 projections).

## Results (2026-10-11, this repo)

Windows (MSVC, 6-thread laptop CPU):

| Stage (10 s audio) | Median time | Realtime multiple |
|---|---|---|
| Log-mel frontend | 11.63 ms | 860x |
| TinyEncoder forward | 114.47 ms | 87x |

| End-to-end input | Wall time | RTF (lower is better) |
|---|---|---|
| 1 s | 25.5 ms | **0.026** |
| 5 s | 200.7 ms | **0.040** |
| 10 s | 407.2 ms | **0.041** |
| 30 s | 1088.4 ms | **0.036** |

Linux (WSL2 Ubuntu 26.04, GCC 15.2, same host hardware, `linux-ninja-release`):

| Stage (10 s audio) | Median time | Realtime multiple |
|---|---|---|
| Log-mel frontend | 6.56 ms | 1524x |
| TinyEncoder forward | 40.19 ms | 249x |

| End-to-end input | Wall time | RTF (lower is better) |
|---|---|---|
| 1 s | 14.7 ms | **0.015** |
| 5 s | 96.2 ms | **0.019** |
| 10 s | 215.7 ms | **0.022** |
| 30 s | 727.5 ms | **0.024** |

Linux additionally verified: 53/53 test assertions pass under GCC 15 with
`-Wall -Wextra -Wshadow` and zero warnings; `cmake --install` + `pkg-config`
round-trip proven by compiling and running an external consumer against the
installed `libspoke.so` (only the 7 `spoke_*` C ABI symbols exported);
`DESTDIR` staging produces the versioned `libspoke.so → .so.0 → .so.0.1.0` chain.

Interpretation: spoke transcribes roughly **25x faster than realtime** on a
plain CPU -- a 30-second clip finishes in ~1.1 s. The frontend is essentially
free (860x); the encoder dominates, as expected, and still clears 87x.

Raw harness output is persisted to `results_bench.txt` on every run:

```
frontend_10s_ms=11.6273
encoder_10s_ms=114.471
params=310813
threads=6
```

## How to reproduce

```powershell
cmake --build build --config Release --parallel
.\build\Release\spoke_bench.exe
ctest --test-dir build -C Release --output-on-failure  # 53/53 green gates perf sanity (RTF<1.0 asserts)
```

## Optimization ledger

1. Sparse mel filterbank (only nonzero taps) + precomputed FFT twiddles.
2. Per-row int8 quantization of both projections (memory traffic ÷ ~4).
3. 4x-unrolled, auto-vectorized dot products; cache-blocked matmul.
4. ThreadPool capped at 8 workers (tiny-model saturation point).
5. Zero-copy chunking at 30 s; thread-local FFT scratch (no hot-loop allocs).
6. Confidence-gated early skip: silent VAD regions never reach the encoder.
