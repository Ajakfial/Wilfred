#!/usr/bin/env python3
"""
Generate CHANGELOG.md from git commits.
Usage: python3 .agents/scripts/gen-changelog.py [--since TAG] [--output FILE]
"""
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT_FILE = ROOT / "CHANGELOG.md"

# Commit type mapping
TYPE_MAP = {
    "feat": "Features",
    "fix": "Bug Fixes",
    "docs": "Documentation",
    "style": "Style",
    "refactor": "Refactoring",
    "test": "Tests",
    "chore": "Chores",
    "build": "Build",
    "ci": "CI",
    "perf": "Performance",
}

def get_commits(since=None):
    """Get commits since tag or all."""
    cmd = ["git", "log", "--pretty=format:%H|%s|%an|%ad", "--date=short"]
    if since:
        cmd.append(f"{since}..HEAD")
    else:
        cmd.append("--all")
    result = subprocess.run(cmd, capture_output=True, text=True, cwd=Path.cwd())
    if result.returncode != 0:
        return []
    
    commits = []
    for line in result.stdout.strip().split("\n"):
        if not line:
            continue
        parts = line.split("|", 3)
        if len(parts) == 4:
            commits.append({
                "hash": parts[0][:7],
                "subject": parts[1],
                "author": parts[2],
                "date": parts[3],
            })
    return commits

def categorize(subject):
    """Categorize commit by conventional prefix."""
    match = re.match(r"^(feat|fix|docs|style|refactor|test|chore|build|ci|perf)(\(.+\))?: ", subject)
    if match:
        return match.group(1)
    # Heuristics for non-conventional
    if subject.startswith(("Add ", "Implement ")):
        return "feat"
    if subject.startswith(("Fix ", "Fix:")):
        return "fix"
    if subject.startswith(("Update ", "Update:")):
        return "chore"
    if subject.startswith(("Refactor ", "Refactor:")):
        return "refactor"
    if subject.startswith(("Doc ", "Docs:", "Document ")):
        return "docs"
    return "chore"

def format_changelog(commits):
    """Group commits by type and format as markdown."""
    sections = {}
    for c in commits:
        cat = categorize(c["subject"])
        sections.setdefault(cat, []).append(c)
    
    lines = [
        "# Changelog",
        "",
        f"All notable changes to Wilfred. Generated {datetime.now().strftime('%Y-%m-%d')}.",
        "",
    ]
    
    for cat in ["feat", "fix", "perf", "docs", "refactor", "test", "build", "ci", "chore"]:
        if cat not in sections:
            continue
        lines.append(f"## {TYPE_MAP.get(cat, cat.title())}")
        lines.append("")
        for c in sections[cat]:
            lines.append(f"- {c['subject']} ([{c['hash']}](https://github.com/Ajakfial/Wilfred/commit/{c['hash']})) — {c['author']} ({c['date']})")
        lines.append("")
    
    return "\n".join(lines)

def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--since", help="Tag to start from (e.g., v42.0.0)")
    parser.add_argument("--output", help="Output file (default: CHANGELOG.md)")
    args = parser.parse_args()
    
    commits = get_commits(args.since)
    if not commits:
        print("No commits found")
        return 1
    
    md = format_changelog(commits)
    
    out_file = Path(args.output) if args.output else Path("CHANGELOG.md")
    out_file.write_text(md)
    print(f"Wrote {out_file} ({len(commits)} commits)")
    return 0

if __name__ == "__main__":
    sys.exit(main())