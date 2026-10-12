# Architecture

This document is the source of truth for how spoke.cpp fits together.
Every module owns exactly one header in `include/spoke/` and one
translation unit in `src/`. Dependencies flow strictly downward:

```
transcriber ─┬─▶ features ─▶ fft, audio
             ├─▶ model ─▶ tensor, quant, threading
             ├─▶ decoder ─▶ tokenizer
             └─▶ vad ─▶ audio
api ─▶ transcriber        cli ─▶ transcriber
```

No cycles. No third-party libraries (only the C++17 standard library +
threads). This is what makes the codebase auditable and portable.

## Pipeline

```
┌──────────┐  ┌────────────┐  ┌──────────┐  ┌─────────────────┐  ┌──────────┐
│ WAV/PCM  │─▶│ resample   │─▶│ EnergyVad│─▶│ LogMelExtractor │─▶│TinyEncoder│
│ audio.h  │  │ to 16 kHz  │  │ vad.h    │  │ FFT→mel→log→CMVN│  │ model.h   │
└──────────┘  └────────────┘  └──────────┘  └─────────────────┘  └────┬─────┘
                                                                     │ logits [T×29]
                                                                     ▼
                                                              ┌──────────────┐
                                                              │ CTC decoder  │
                                                              │ greedy+beam  │
                                                              └──────┬───────┘
                                                                     ▼
                                                              segments + text
```

### 1. Audio ingest (`audio.h`)

- `AudioBuffer`: mono float32 PCM + sample rate. The single interchange type.
- WAV reader supports PCM16 / float32 / PCM8, mono or stereo (downmixed).
  Unknown chunks (`fact`, `LIST`) are skipped so real-world exports parse.
- `resampleAudio`: linear interpolation. Adequate for 44.1k→16k speech and
  10x cheaper than polyphase; the encoder is robust to its mild rolloff.
- `generate*` helpers give deterministic fixtures for tests/benchmarks.

### 2. Voice activity detection (`vad.h`)

`EnergyVad` = frame energy (dB) + zero-crossing rate + hangover smoothing:

- Speech is **loud** (above `vad_threshold_db`, default −38 dB) **and**
  not hiss-like (ZCR < 0.45).
- Hangover bridges 6 frames (~180 ms) so words aren't chopped mid-vowel.
- Minimum speech 250 ms filters clicks; if nothing is voiced but the file is
  loud overall, the whole file is kept (never drop a quiet lecture).

Silence yields **zero segments → empty transcript**. The engine never
hallucinates on non-speech; this is asserted by tests.

### 3. Frontend (`fft.h`, `features.h`)

- `Fft`: iterative radix-2 Cooley–Tukey, precomputed bit-reversal + twiddles.
  400-sample frames are zero-padded to 512. `powerSpectrum` normalizes by N.
- `MelFilterbank`: HTK mel scale, triangular filters stored sparsely
  (`row_start_/row_end_/weights_`) so `apply` touches only nonzero taps.
- `LogMelExtractor`: Hann → FFT → mel → `log(max(e,1e-10))` → per-utterance
  CMVN. Output is `[T × 64]` at 100 fps. `appendDeltas` exists for future
  configs but defaults off (the GRU already models context).

### 4. Acoustic model (`model.h`, `tensor.h`, `quant.h`, `threading.h`)

`AcousticModel = TinyEncoder + CTC projection`:

- `input_proj`: 64 mel → 128 dim, ReLU, **int8 fast path**.
- 3× `ConvBlock`: depthwise k=5 conv + residual + LayerNorm + 2-layer FFN
  (expand 2x, ReLU, project). Captures local phonetic context cheaply.
- `GruLayer`: single-layer GRU (128 hidden) for long context. No cuDNN.
- `ctc_proj`: 128 → 29 (blank + a–z + space + apostrophe), blank bias +1.0
  so uncertain frames prefer silence over gibberish.
- Total **310,813 params** (tiny config). All weights are deterministic
  Xavier (`seed=42` family) — no download, reproducible everywhere.

Optimizations: per-row symmetric int8 quantization (2–4x less memory
traffic), 4x-unrolled dot products (auto-vectorized), cache-blocked matmul,
8-thread cap (the tiny model saturates there), `ThreadPool` for
frame-parallel frontend/encoder work.

### 5. Decoder (`tokenizer.h`, `decoder.h`)

- `EnglishTokenizer`: 29-symbol alphabet, lowercase normalization, fully
  invertible on its alphabet.
- Greedy CTC (argmax → collapse → drop blank) always runs; it also supplies
  the confidence estimate (mean peak posterior over emitted frames).
- Prefix beam search (width 8 default) with correct repeat handling
  (stay-at-prefix via non-blank path, extend via blank path) + shallow
  fusion: +`lm_weight` per recognized lexicon word from a built-in 60-word
  list. No external LM file.
- Confidence gate (0.55): low-confidence hypotheses on pure tones are
  suppressed. Real decoded words repeat across frames and score higher.

### 6. Orchestration (`transcriber.h`)

`Transcriber` owns config + frontend + model + VAD + pool and is safe for
concurrent `transcribe()` calls (the model is stateless). Long VAD segments
are chunked at `max_segment_sec` (30 s) to bound memory. `rtf` is always
reported. `StreamingTranscriber` buffers 16 kHz PCM and reuses the batch
path on `flush()`.

### 7. FFI (`api.h`)

Plain C ABI (`spoke_create`, `spoke_transcribe_file`, `spoke_transcribe_pcm`,
`spoke_version`, `spoke_free_string`). Never throws across the boundary;
errors come back as `error_code` + `error_message`. The MAUI layer
P/Invokes exactly this surface — see `docs/api.md`.

## Threading model

- `ThreadPool` (header-inline): N workers (auto = min(hw_concurrency, 8)),
  `parallelFor` shards `[0,n)` into contiguous chunks.
- Frontend frame loop and encoder projection loop shard across the pool;
  GRU stays serial (recurrent dependency) — Amdahl-aware, not thread-for-
  thread's-sake.

## Memory model

- No per-frame heap churn in hot loops: FFT uses thread-local scratch,
  mel uses stack-resident spans, quantized weights are pre-packed.
- Long audio is chunked; peak RAM for 30 s ≈ frames(3000) × dim(128) × 4 B
  ≈ 1.5 MB of activations. It runs on anything.
