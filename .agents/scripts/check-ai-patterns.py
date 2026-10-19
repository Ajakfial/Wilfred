#!/usr/bin/env python3
"""
Check for AI-generated code patterns in source files.
Usage: python3 .agents/scripts/check-ai-patterns.py [--fix] [path...]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# Patterns that suggest AI-generated code
AI_PATTERNS = [
    # Overly verbose comments
    (r"// This function does X", "Verbose obvious comment"),
    (r"/\*\s*\n\s*\* @param\s+\w+\s+\w+\s*\n\s*\* @return", "Verbose Javadoc"),
    
    # AI-style patterns
    (r"// TODO: Implement", "Placeholder TODO"),
    (r"// FIXME:", "FIXME comment"),
    
    # Overly defensive coding
    (r"if \(.* == nullptr\) \{ return false; \}", "Defensive null check"),
    (r"if \(.* == nullptr\) return false;", "Inline null check"),
    
    # AI loves these patterns
    (r"std::make_unique<[^>]+>\(\)", "make_unique with empty parens"),
    (r"std::make_shared<[^>]+>\(\)", "make_shared with empty parens"),
    
    # AI loves early returns
    (r"if \(!condition\) return false;", "Early return pattern"),
    
    # AI loves structured bindings
    (r"auto \[.*\] = ", "Structured binding"),
    
    # AI loves ranged-for
    (r"for \(auto& .* : .*\)", "Range-based for"),
    
    # AI loves constexpr
    (r"constexpr auto ", "constexpr auto"),
]

def check_file(filepath):
    """Check a single file for AI patterns."""
    try:
        text = filepath.read_text(encoding="utf-8")
    except UnicodeDecodeError:
        return []
    
    hits = []
    for line_num, line in enumerate(text.splitlines(), 1):
        for pattern, desc in AI_PATTERNS:
            if re.search(pattern, line):
                hits.append((line_num, line.strip()[:100], desc))
    return hits

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("paths", nargs="*", default=["src", "include", "tests"])
    parser.add_argument("--fix", action="store_true", help="Attempt to fix (not implemented)")
    args = parser.parse_args()
    
    total_hits = 0
    for path_str in args.paths:
        path = Path(path_str)
        if not path.exists():
            continue
        for filepath in path.rglob("*.cpp"):
            if "build" in filepath.parts or ".git" in filepath.parts:
                continue
            hits = check_file(filepath)
            if hits:
                print(f"\n{filepath.relative_to(Path.cwd())}:")
                for line_num, line, desc in hits:
                    print(f"  Line {line_num}: [{desc}] {line[:100]}")
                    total_hits += 1
    
    if total_hits:
        print(f"\n⚠️  Found {total_hits} potential AI patterns")
        return 1
    else:
        print("✅ No obvious AI patterns detected")
        return 0

if __name__ == "__main__":
    sys.exit(main())