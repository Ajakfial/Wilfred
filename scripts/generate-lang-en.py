#!/usr/bin/env python3
"""Regenerate lang/en.yml from the compiled-in table in src/locale/locale.cpp.

Usage (from the repo root):
    python3 scripts/generate-lang-en.py

lang/en.yml is the reference catalog: translators copy it to lang/<code>.yml
and translate the values. Regenerate after changing kEnglish so the reference
never drifts from what tr() actually falls back to.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "src" / "locale" / "locale.cpp"
OUT = ROOT / "lang" / "en.yml"

LIT = r'"(?:[^"\\]|\\.)*"'


def main() -> int:
    body = SRC.read_text(encoding="utf-8")
    m = re.search(r"kEnglish\[\] = \{(.*?)\n\};", body, re.S)
    if not m:
        print("kEnglish table not found", file=sys.stderr)
        return 1
    entries = re.findall(r"\{(" + LIT + r")\s*,\s*((?:" + LIT + r"\s*)+)\}", m.group(1))
    print(f"entries found: {len(entries)}")
    lines = [
        "# Wilfred English reference catalog (generated from src/locale/locale.cpp).",
        "# Regenerate: python3 scripts/generate-lang-en.py",
        "# Copy this file to <code>.yml and translate the values to add a language.",
        "# Rules: quote every value; keep {placeholders} and command syntax intact;",
        "# never change the keys. See docs/localization.md.",
        "",
    ]
    for k_raw, v_raw in entries:
        key = json.loads(k_raw)
        val = "".join(json.loads(p) for p in re.findall(LIT, v_raw))
        esc = val.replace("\\", "\\\\").replace('"', '\\"')
        lines.append(f'{key}: "{esc}"')
    OUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
