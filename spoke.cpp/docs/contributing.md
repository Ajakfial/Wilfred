# Contributing

## Module discipline

- One module = one header + one TU. Include only downward in the
  `docs/architecture.md` dependency DAG. New DSP goes in `features.h`,
  new layers in `model.h`, new search in `decoder.h`.
- No new third-party dependencies without an ADR. The zero-dependency
  promise is the project's identity.

## Definition of done for engine PRs

1. Clean configure + build on your OS (`scripts/build-*.sh|ps1`), zero new warnings
   on MSVC, GCC and AppleClang alike.
2. `ctest` 48/48 green -- extend `tests/test_all.cpp` for new behaviour
   (deterministic fixtures only, no network, no checked-in audio blobs).
3. `spoke_bench` shows no RTF regression (RTF must stay < 1.0; tiny target
   is ≤ 0.1 on a laptop CPU).
4. Docs updated (`architecture.md` for structure, `model.md` for behaviour).

## Roadmap (good first issues)

- [ ] CTC loss + LibriSpeech-trainable `tools/train_stub` (keep init path).
- [ ] 8 kHz telephony frontend mode + resample quality flag.
- [ ] Quantized GRU recurrent matrices (currently only projections are int8).
- [ ] `--stream` CLI mode demoing `StreamingTranscriber` over stdin PCM.
- [ ] Android `jniLibs` CI job building `libspoke.so` per ABI.
- [ ] Golden transcript fixtures (hand-verified short clips, MIT-licensed).
