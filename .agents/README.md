# .agents — AI Exception Workflows

This directory contains the workflows, scripts, and instructions for the **very few exceptions** where AI assistance is explicitly permitted per `AGENTS.md`.

## Contents

| File | Purpose |
|------|---------|
| `allowed.md` | What AI can do (and how) |
| `forbidden.md` | What AI must never do |
| `workflows.md` | Step-by-step workflows for each exception |
| `scripts/` | Helper scripts for allowed tasks |
| `templates/` | Code templates for common tasks |
| `skills/` | Loadable skills for Claude Code and other AI agents |

## Skills

`skills/` contains task-scoped `SKILL.md` packs an agent can load on demand:
policy gate, project orientation, documentation, tests, minor minis,
localization, build/CI, and commit/PR. See
[`skills/README.md`](skills/README.md) for the format and how to expose the
directory to Claude Code (`.claude/skills/`).

Always load `skills/wilfred-ai-policy/SKILL.md` first.

## Tool entrypoints

`AGENTS.md` is the canonical policy. These files point to it or keep forbidden
paths out of an AI tool's context:

| File | Purpose |
|------|---------|
| `CLAUDE.md` | Claude Code project instructions |
| `GEMINI.md` | Gemini CLI project instructions |
| `.github/copilot-instructions.md` | GitHub Copilot repository instructions |
| `.cursorignore` | Cursor (reference pattern list) |
| `.geminiignore` | Gemini CLI |
| `.aiderignore` | Aider |
| `.codeiumignore` | Codeium / Windsurf |
| `.aiexclude` | Google AI / Gemini Code Assist |
| `.clineignore` | Cline |
| `.rooignore` | Roo Code |
| `.augmentignore` | Augment Code |
| `.copilotignore` | GitHub Copilot (best effort) |
| `.antigravityignore` | Google Antigravity |

The ignore files mirror `.cursorignore`. Update that file first, then copy the
same pattern list to the others. They are advisory and never replace human
review.

## Quick Reference

| Task | Allowed? | See |
|------|----------|-----|
| Docs from bullets | ✅ | `allowed.md#documentation` |
| Minor minis | ✅ | `allowed.md#minor-minis` |
| Small fixes | ✅ | `allowed.md#small-fixes` |
| Moderate refactors | ✅ | `allowed.md#moderate-refactors` |
| Test additions | ✅ | `allowed.md#test-additions` |
| Build/CI tweaks | ✅ | `allowed.md#buildci-tweaks` |
| Localization | ✅ | `allowed.md#localization` |
| Core algorithms | ❌ | `forbidden.md` |
| Platform internals | ❌ | `forbidden.md` |
| System integration | ❌ | `forbidden.md` |
| Architecture decisions | ❌ | `forbidden.md` |
| Security code | ❌ | `forbidden.md` |
| Performance paths | ❌ | `forbidden.md` |
| AI-generated PRs | ❌ | `forbidden.md` |

---

**Rule of thumb:** If you have to ask "is this allowed?" — it's probably not.