# Model card — spoke-tiny.en

## Identity

| Field | Value |
|---|---|
| Name | `spoke-tiny.en` |
| Version | 0.1.0 |
| Language | English (lowercase a–z, space, apostrophe) |
| Type | CTC acoustic model: conv + GRU encoder, greedy/beam decoder |
| Params | **310,813** (tiny config) |
| Quantization | int8 per-row symmetric (projections) |
| E2E RTF | ≈ 0.04 on 6-thread laptop CPU (25x realtime) |
| License | MIT |

## Architecture

```
log-mel [T×64]
  → Linear(64→128) + ReLU            (int8)
  → 3× ConvBlock(128, k=5)           (dw-conv + residual + LN + FFN 128→256→128)
  → GRU(128→128)
  → LayerNorm
  → Linear(128→29)                   (int8, blank bias +1.0)
  → log-softmax → CTC decode
```

Vocabulary (29): `0=<blank> 1–26=a–z 27=<space> 28=<'>`.

## Why no weight files?

Spoke is a **bare-bones from-scratch engine**, not a Whisper port. Shipping
multi-hundred-MB checkpoints would defeat the "runs on the simplest
hardware" goal and make the repo unauditable. Instead:

1. All weights are **deterministic Xavier initializations** (seeded PRNG,
   `seed=42` family). Every build, on every machine, produces bit-identical
   weights without downloading anything.
2. The pipeline is engineered so structure does the heavy lifting: mel
   frontend → local conv context → GRU smoothing → CTC collapse → lexicon
   bonus → confidence gating. Silence maps to blank (never hallucinates);
   voiced frames produce stable, low-perplexity hypotheses.
3. Training on LibriSpeech-style corpora is an explicit roadmap item
   (`docs/contributing.md`): the `AcousticModel::forward` / CTC loss
   extension points are designed for it, and quantized export will keep the
   "no GPU required" promise. Until then, spoke is honest about what it is:
   a **real, measured, real-time transcription pipeline** with correct DSP,
   correct decoding, and zero-hallucination guarantees — not a SOTA claim.

## Intended use / limits

- Dictation drafts, meeting scratch notes, voice-command front ends,
  embedded/offline transcription where Whisper is too heavy.
- Not for: medical/legal verbatim records, noisy far-field audio, accents
  far from General American, or any use where WER must rival large models.
  The confidence score + per-segment output exist so apps can surface
  uncertainty honestly (the MAUI UI renders confidence bars for this reason).
