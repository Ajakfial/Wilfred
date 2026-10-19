# models/

Spoke ships **no binary weight files by design**. The tiny English model
(`spoke-tiny.en`, 310,813 params) is deterministically generated at engine
construction via seeded Xavier initialization -- see `src/model.cpp`
(`TinyEncoder`, `AcousticModel`) and the full spec in `model_card.md`.

- `model_card.md` -- architecture, vocab, quantization, intended use/limits.
- `export_reference.py` -- regenerates the first N weights with an
  independent Python implementation of the same Xavier scheme so auditors
  can verify the C++ engine bit-for-bit without building anything.

To add a trained checkpoint later: implement `AcousticModel::save/load`
against a versioned binary format, keep the seeded fallback, and document
the training corpus + WER in the model card. Never break offline builds.
