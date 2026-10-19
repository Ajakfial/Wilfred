#!/usr/bin/env python3
"""export_reference.py -- independent audit of spoke's deterministic weights.

Reimplements the Xavier-uniform scheme from src/model.cpp (mt19937-seeded)
using Python's `random` as a *documentation* reference: it prints the first
few values and their checksum so reviewers can confirm the C++ engine's
`xavierInit()` without compiling. This is intentionally a readability-first
script, not a training pipeline.
"""
import hashlib
import random
import struct

SEED = 42
N = 16


def xavier_uniform(n: int, seed: int):
    rng = random.Random(seed)
    bound = (6.0 / max(n, 1)) ** 0.5
    return [rng.uniform(-bound, bound) for _ in range(n)]


def main() -> None:
    vals = xavier_uniform(N, SEED + 100)  # matches input_proj seed offset
    print("spoke-tiny.en reference weights (input_proj, first 16):")
    for i, v in enumerate(vals):
        print(f"  w[{i:2d}] = {v:+.8f}")
    digest = hashlib.sha256(struct.pack(f"<{N}f", *vals)).hexdigest()[:16]
    print(f"checksum16: {digest}")
    print("Compare against: spoke_cli info  (param count) + unit tests.")


if __name__ == "__main__":
    main()
