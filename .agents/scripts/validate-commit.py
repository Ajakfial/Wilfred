#!/usr/bin/env python3
"""
Validate commit message for AI policy compliance.
Usage: python3 .agents/scripts/validate-commit.py <commit-message-file>
"""
import re
import sys
from pathlib import Path

AI_COMMIT_PATTERNS = [
    r"^(feat|fix|docs|style|refactor|test|chore)\([a-z-]+\): ",
    r"^(Add|Fix|Update|Remove|Refactor|Implement|Improve|Optimize) ",
    r"^(feat|fix|docs|style|refactor|test|chore):$",
]

AI_CONTENT_PATTERNS = [
    r"```[a-z]*\n",
    r"## ",
    r"### ",
]

def check_message(msg):
    issues = []
    
    # Strip comments
    lines = [l for l in msg.splitlines() if not l.startswith("#")]
    msg_clean = "\n".join(lines)
    
    # Check AI patterns
    for pattern in AI_COMMIT_PATTERNS:
        if re.search(pattern, msg_clean.strip()):
            issues.append(f"Matches AI pattern: {pattern}")
    
    bullets = len(re.findall(r"^[-*]\s", msg_clean, re.MULTILINE))
    if bullets > 3:
        issues.append(f"Too many bullet points ({bullets})")
    
    for pattern in AI_CONTENT_PATTERNS:
        if re.search(pattern, msg_clean):
            issues.append(f"Markdown formatting: {pattern}")
    
    lines = [l for l in msg_clean.splitlines() if l.strip()]
    if len(lines) > 15:
        issues.append(f"Too long ({len(lines)} lines)")
    
    # Check for "Human-authored" in body (required for PRs)
    if "Human-authored:" not in msg_clean:
        issues.append("Missing 'Human-authored: yes' in message body")
    
    return issues

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 validate-commit.py <commit-message-file>")
        return 1
    
    msg_file = Path(sys.argv[1])
    if not msg_file.exists():
        print(f"File not found: {msg_file}")
        return 1
    
    msg = msg_file.read_text(encoding="utf-8", errors="ignore")
    issues = check_message(msg)
    
    if issues:
        print("❌ Commit message validation failed:")
        for issue in issues:
            print(f"  - {issue}")
        print("\n✍️  Write your own commit message. Include 'Human-authored: yes'")
        return 1
    
    print("✅ Commit message validated")
    return 0

if __name__ == "__main__":
    sys.exit(main())