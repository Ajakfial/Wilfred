#!/usr/bin/env python3
"""
Find and optionally fix common typos in source files.
Usage:
  python3 .agents/scripts/fix-typos.py --check
  python3 .agents/scripts/fix-typos.py --fix
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC_DIRS = [ROOT / "src", ROOT / "include", ROOT / "tests", ROOT / "docs"]

# Common typos: (pattern, replacement, description)
TYPO_RULES = [
    (r"\bteh\b", "the", "teh -> the"),
    (r"\brecieve\b", "receive", "recieve -> receive"),
    (r"\bseperate\b", "separate", "seperate -> separate"),
    (r"\boccured\b", "occurred", "occured -> occurred"),
    (r"\baccomodate\b", "accommodate", "accomodate -> accommodate"),
    (r"\bdefinately\b", "definitely", "definately -> definitely"),
    (r"\boccurence\b", "occurrence", "occurence -> occurrence"),
    (r"\bpriviledge\b", "privilege", "priviledge -> privilege"),
    (r"\bdependant\b", "dependent", "dependant -> dependent"),
    (r"\brecomend\b", "recommend", "recomend -> recommend"),
    (r"\bneccessary\b", "necessary", "neccessary -> necessary"),
    (r"\boccuring\b", "occurring", "occuring -> occurring"),
    (r"\bpublically\b", "publicly", "publically -> publicly"),
    (r"\baccomodation\b", "accommodation", "accomodation -> accommodation"),
    (r"\baready\b", "already", "aready -> already"),
    (r"\bchangable\b", "changeable", "changable -> changeable"),
    (r"\bcompatable\b", "compatible", "compatable -> compatible"),
    (r"\bexistance\b", "existence", "existance -> existence"),
    (r"\bformely\b", "formerly", "formely -> formerly"),
    (r"\bhappend\b", "happened", "happend -> happened"),
    (r"\bimmediatly\b", "immediately", "immediatly -> immediately"),
    (r"\bmaintainance\b", "maintenance", "maintainance -> maintenance"),
    (r"\boccurence\b", "occurrence", "occurence -> occurrence"),
    (r"\bperfer\b", "prefer", "perfer -> prefer"),
    (r"\bpersue\b", "pursue", "persue -> pursue"),
    (r"\bquestionaire\b", "questionnaire", "questionaire -> questionnaire"),
    (r"\brecomended\b", "recommended", "recomended -> recommended"),
    (r"\brefering\b", "referring", "refering -> referring"),
    (r"\bsepereate\b", "separate", "sepereate -> separate"),
    (r"\bsuprise\b", "surprise", "suprise -> surprise"),
    (r"\btheirselves\b", "themselves", "theirselves -> themselves"),
    (r"\buntill\b", "until", "untill -> until"),
    (r"\bwhereas\b", "whereas", "whereas (no change)"),
]

def check_file(filepath):
    """Check a single file for typos. Returns list of (line_num, line, rule_desc)."""
    hits = []
    try:
        text = filepath.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return hits

    for line_num, line in enumerate(text.splitlines(), 1):
        for pattern, repl, desc in TYPO_RULES:
            if re.search(pattern, line, re.IGNORECASE):
                hits.append((line_num, line.strip(), desc))
    return hits

def fix_file(filepath):
    """Fix typos in a single file. Returns number of fixes."""
    try:
        text = filepath.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return 0

    original = text
    fixes = 0
    for pattern, repl, _ in TYPO_RULES:
        text, n = re.subn(pattern, repl, text, flags=re.IGNORECASE)
        fixes += n

    if fixes > 0:
        filepath.write_text(text, encoding="utf-8")
    return fixes

def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ("--check", "--fix"):
        print("Usage: python3 fix-typos.py --check|--fix")
        sys.exit(1)

    mode = sys.argv[1]
    total_hits = 0
    total_files = 0

    for src_dir in SRC_DIRS:
        if not src_dir.exists():
            continue
        for filepath in src_dir.rglob("*.cpp"):
            if "build" in filepath.parts:
                continue
            hits = check_file(filepath)
            if hits:
                total_files += 1
                total_hits += len(hits)
                if mode == "--check":
                    print(f"\n{filepath.relative_to(ROOT)}:")
                    for line_num, line, desc in hits:
                        print(f"  Line {line_num}: {desc}")
                        print(f"    {line[:100]}")
                elif mode == "--fix":
                    n = fix_file(filepath)
                    if n:
                        print(f"Fixed {n} typos in {filepath.relative_to(ROOT)}")

    if mode == "--check":
        print(f"\nTotal: {total_hits} typos in {total_files} files")
    else:
        print(f"Done. Fixed typos in {total_files} files.")

if __name__ == "__main__":
    main()