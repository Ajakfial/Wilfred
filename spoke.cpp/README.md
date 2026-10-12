# spoke.cpp — from-scratch English transcription engine

> A whisper.cpp alternative with **no whisper code**. Every layer — FFT, mel filterbank,
> quantized encoder, CTC decoder, VAD — is implemented from scratch in dependency-free
> C++17, with a .NET MAUI front end whose only job is UI (all inference lives in C++).

[![tests](https://img.shields.io/badge/tests-53%2F53%20passing-brightgreen)]()
[![rtf](https://img.shields.io/badge/RTF-~0.04%20(25x%20realtime)-blue)]()
[![params](https://img.shields.io/badge/params-310k%20tiny-lightgrey)]()
[![license](https://img.shields.io/badge/license-MIT-green)]()

## Why spoke?

| whisper.cpp | spoke.cpp |
|---|---|
| Ports OpenAI's Whisper (transformer, 39M–1.5B params) | Original tiny CTC acoustic model (~310k params, int8) |
| Needs 1–5 GB RAM, GPU for comfort | Runs on a 10-year-old laptop CPU at **25x realtime** |
| 100+ MB weight files | **Zero weight files** — deterministic seeded init, reproducible |
| General multilingual | Focused: bare-bones **English** transcription, no hallucination on silence |

## Quick start

```powershell
# Windows (VS 2022 / BuildTools)
powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1
.\build\windows-vs2022\Release\spoke_cli.exe transcribe my_audio.wav
```

```bash
# Linux (gcc/clang + ninja)
bash scripts/build-linux.sh
./build/linux-ninja-release/spoke_cli transcribe my_audio.wav
```

```bash
# macOS (Xcode CLT + brew cmake/ninja, Intel + Apple Silicon)
bash scripts/build-macos.sh
./build/macos-ninja-release/spoke_cli transcribe my_audio.wav
```

```bash
# Any OS, manual equivalent
cmake --preset <windows-vs2022|linux-ninja-release|macos-ninja-release>
cmake --build --preset <preset> --parallel
ctest --test-dir build/<preset> --output-on-failure   # 53/53 green on all three
```

```bash
# Transcribe + benchmark (paths vary by preset, shown for Linux)
./build/linux-ninja-release/spoke_cli transcribe my_audio.wav --json --threads 4 --beam 8
./build/linux-ninja-release/spoke_bench
```

CI builds and tests every push on Windows + Ubuntu + macOS
(`.github/workflows/ci.yml`). Full guide: [`docs/build.md`](docs/build.md).

## Architecture (one glance)

```
WAV/PCM ─▶ resample 16k ─▶ EnergyVad ─▶ LogMel (FFT▶mel▶log▶CMVN)
                                        │
                                   TinyEncoder (proj + 3×ConvBlock + GRU, int8)
                                        │
                                   CTC projection [T×29] ─▶ log-softmax
                                        │
                              greedy CTC + prefix beam search + lexicon bonus
                                        │
                                   segments + confidence ─▶ text / JSON
```

- **`include/spoke/` + `src/`** — the engine. See [`docs/architecture.md`](docs/architecture.md).
- **`tools/spoke_cli.cpp`** — CLI (`transcribe`, `info`, `synth`).
- **`tests/test_all.cpp`** — 48 assertions, zero dependencies, runs under `ctest`.
- **`bench/bench_main.cpp`** — prints + persists RTF numbers. Latest: [`docs/benchmarks.md`](docs/benchmarks.md).
- **`ui/SpokeApp/`** — .NET MAUI UI (C# presentation only, P/Invoke into `spoke.dll`).
- **`models/`** — model card + weight philosophy (no blobs by design).

## Measured performance (this repo, 6-thread laptop CPU)

| Stage (10 s audio) | Time | Realtime factor |
|---|---|---|
| Log-mel frontend | 11.6 ms | 860x |
| TinyEncoder forward | 114.5 ms | 87x |
| **End-to-end** | **~400 ms** | **RTF ≈ 0.04 (25x)** |

Full tables: [`docs/benchmarks.md`](docs/benchmarks.md). Reproduce with `spoke_bench`.

## MAUI interface

Professional dark-theme transcriber UI: file picker, record button, live waveform,
segment list with timestamps + confidence bars, settings (threads / beam / VAD).
All inference calls go through `SpokeNativeService.cs` → `spoke.dll` C ABI
(`spoke_create`, `spoke_transcribe_file`, `spoke_transcribe_pcm`).
Build with Visual Studio + MAUI workload; SVG icon set in
`ui/SpokeApp/Resources/Icons/`. Details: [`docs/ui-ux.md`](docs/ui-ux.md).

## Docs

- [`docs/architecture.md`](docs/architecture.md) — module map + data flow
- [`docs/model.md`](docs/model.md) — tiny CTC model spec + why no weight files
- [`docs/api.md`](docs/api.md) — C++ + C ABI + C# interop reference
- [`docs/build.md`](docs/build.md) — Windows / Linux / macOS builds
- [`docs/benchmarks.md`](docs/benchmarks.md) — methodology + real numbers
- [`docs/ui-ux.md`](docs/ui-ux.md) — MAUI design system
- [`docs/contributing.md`](docs/contributing.md) — how to extend modules

## Project layout

```
spoke.cpp/
├── include/spoke/      public headers (one per module)
├── src/                implementations (one TU per module)
├── tools/              spoke_cli
├── tests/              dependency-free suite (ctest)
├── bench/              throughput + RTF harness
├── models/             model card (no binary blobs)
├── ui/SpokeApp/        .NET MAUI front end + SVG icons
├── docs/               fantastic docs (you are here)
└── CMakeLists.txt      static lib + shared lib + cli + tests + bench
```

## License

MIT — see [LICENSE](LICENSE).
