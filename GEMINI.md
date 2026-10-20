# Gemini CLI Instructions

`AGENTS.md` is the authoritative AI policy for this repository. Read it before
any edit. This file is a pointer to the detailed rules, not a replacement.

`.agents/ai-ignore/.geminiignore` mirrors `.agents/ai-ignore/.cursorignore`
and keeps the forbidden paths out of Gemini CLI's context. Gemini CLI only
reads the file at the repository root, so copy it there before a session
(`Copy-Item .agents\ai-ignore\.geminiignore .` on Windows, or
`cp .agents/ai-ignore/.geminiignore .` elsewhere). It is advisory; the policy
still applies.

## Before editing

1. Read `.agents/skills/wilfred-ai-policy/SKILL.md`.
2. Read the matching task skill from `.agents/skills/`.
3. Check the target paths against `.agents/forbidden.md`.

## Allowed exceptions

Documentation, minor minis, small fixes, moderate refactors, test additions,
build/CI tweaks, and localization. Each has a workflow in
`.agents/allowed.md` and a skill where applicable.

## Forbidden

Core algorithms, platform internals, system integration, architecture
decisions, security-sensitive code, and performance-critical paths. Do not
write forbidden code in any form, including drafts, pseudocode, examples, or
tests for forbidden behavior.

## Review and commits

Every AI-assisted change needs line-by-line human review. Do not write commit
messages or PR descriptions, and do not commit or push on the human's behalf.
PR descriptions must include `Human-authored: yes`.
