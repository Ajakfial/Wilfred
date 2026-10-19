#!/usr/bin/env python3
"""
Regenerate docs index from markdown files.
Usage: python3 .agents/scripts/gen-docs-index.py
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ROOT / "docs"
OUT = ROOT / "docs" / "INDEX.md"

def main():
    md_files = sorted(DOCS.glob("*.md"))
    lines = [
        "# Documentation Index",
        "",
        f"Generated: {__import__('datetime').datetime.now().isoformat()}",
        f"Total files: {len(md_files)}",
        "",
        "## Files",
        "",
    ]

    for f in md_files:
        rel = f.relative_to(ROOT)
        title = f.stem.replace("-", " ").title()
        lines.append(f"- [{title}]({rel})")

    OUT.write_text("\n".join(lines) + "\n")
    print(f"Wrote {OUT}")

if __name__ == "__main__":
    main()