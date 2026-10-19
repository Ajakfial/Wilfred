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