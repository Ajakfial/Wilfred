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
| `ai-ignore/.cursorignore` | Cursor (reference pattern list) |
| `ai-ignore/.geminiignore` | Gemini CLI |
| `ai-ignore/.aiderignore` | Aider |
| `ai-ignore/.codeiumignore` | Codeium / Windsurf |
| `ai-ignore/.aiexclude` | Google AI / Gemini Code Assist |
| `ai-ignore/.clineignore` | Cline |
| `ai-ignore/.rooignore` | Roo Code |
| `ai-ignore/.augmentignore` | Augment Code |
| `ai-ignore/.copilotignore` | GitHub Copilot (best effort) |
| `ai-ignore/.antigravityignore` | Google Antigravity |
| `ai-ignore/.sccignore` | scc code counter |

The ignore files mirror `ai-ignore/.cursorignore`. Update that file first, then
copy the same pattern list to the others. They are advisory and never replace
human review.

### Why the ignore files are not at the root

AI tools look for their ignore file in the repository root only, so moving them
into `ai-ignore/` keeps the root tidy at the cost of switching the lists off
until they are copied back. Restore them before an AI session if you want the
tool to honor them:

```bash
cp .agents/ai-ignore/.[a-z]*ignore .
cp .agents/ai-ignore/.aiexclude .
```

```powershell
Copy-Item .agents\ai-ignore\.cursorignore, .agents\ai-ignore\.geminiignore, `
  .agents\ai-ignore\.aiderignore, .agents\ai-ignore\.codeiumignore, `
  .agents\ai-ignore\.aiexclude, .agents\ai-ignore\.clineignore, `
  .agents\ai-ignore\.rooignore, .agents\ai-ignore\.augmentignore, `
  .agents\ai-ignore\.copilotignore, .agents\ai-ignore\.antigravityignore, `
  .agents\ai-ignore\.sccignore .
```

The policy does not change either way: `AGENTS.md` is enforced by review and
the pre-commit hook, not by the ignore lists.

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