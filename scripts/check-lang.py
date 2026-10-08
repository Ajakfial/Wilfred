#!/usr/bin/env python3
"""Check lang/*.yml catalogs: identical key sets, matching the compiled-in
English table in src/locale/locale.cpp, values quoted and non-empty.

Usage (from the repo root):
    python3 scripts/check-lang.py
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def catalog_keys(path: Path) -> list:
    keys = []
    for no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        m = re.match(r'^([A-Za-z0-9_.]+): "(.*)"$', s)
        if not m:
            print(f"{path.name}:{no}: bad line (keys must be dotted, values double-quoted)")
            return []
        if not m.group(2):
            print(f"{path.name}:{no}: empty value for {m.group(1)}")
            return []
        keys.append(m.group(1))
    return keys


def main() -> int:
    src = (ROOT / "src" / "locale" / "locale.cpp").read_text(encoding="utf-8")
    cpp = re.findall(r'^\s*\{"([a-z0-9_.]+)"', src, re.M)
    files = sorted((ROOT / "lang").glob("*.yml"))
    if "lang/en.yml" not in [str(f.relative_to(ROOT)).replace("\\", "/") for f in files]:
        print("missing lang/en.yml")
        return 1
    ok = True
    ref = None
    for f in files:
        ks = catalog_keys(f)
        if ref is None:
            ref = ks
        if ks != ref:
            print(f"{f.name}: key order/set differs from {files[0].name}")
            ok = False
        if len(ks) != len(set(ks)):
            print(f"{f.name}: duplicate keys")
            ok = False
    if ref != cpp:
        missing = [k for k in cpp if k not in (ref or [])]
        extra = [k for k in (ref or []) if k not in cpp]
        print(f"lang files vs compiled table: missing={missing} extra={extra}")
        ok = False
    print(f"keys: {len(ref or [])} cpp: {len(cpp)} -> {'OK' if ok else 'FAIL'}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
