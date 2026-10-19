---
name: wilfred-ai-policy
description: Enforce Wilfred's AI usage policy before any edit. Load this at the start of every task, and whenever a request touches search, ranking, indexing, platform backends, overlay UI, security, performance-critical code, architecture, dependencies, commits, or pull requests.
---

# Wilfred AI Policy Gate

Wilfred is a human-crafted project. AI is allowed for a small, explicit set of
tasks and forbidden everywhere else. This skill is the first thing to load in
any session.

## Read first

1. `AGENTS.md` — the policy.
2. `.agents/forbidden.md` — the zero-tolerance list.
3. `.agents/allowed.md` — the allowed exceptions and their workflows.
4. The relevant task skill under `.agents/skills/`.

## Allowed work (and its skill)

| Task | Skill |
|------|-------|
| Documentation (`docs/`, `README.md`, `*.md`) | `documentation` |
| Minor minis (`src/search/minis.cpp`, trivial additions) | `minor-minis` |
| Small fixes (one-liners, typos, includes) | no skill needed — follow `allowed.md` |
| Moderate refactors (single file, no API/data changes) | no skill needed — follow `allowed.md` |
| Test additions (`tests/`) | `tests` |
| Build/CI tweaks | `build-and-ci` |
| Localization (`lang/*.yml`) | `localization` |

Everything else is forbidden. This includes, but is not limited to:

- Search engine, ranking, fuzzy matching, query parsing, index structures
  (`src/search/`, `src/index/`, `src/query/`)
- Platform backends, filesystem watchers, hotkeys, overlays
  (`src/platform/`, `src/ui/`)
- Autostart, power actions, window management, media keys
- Security-sensitive code (auth, crypto, sandboxing, IPC, plugin trust)
- Performance-critical paths (hot loops, allocators, SIMD, lock-free code)
- Architecture decisions, new modules, new dependencies, data-structure or
  public-API changes

## Rules for every task

1. **Check the target paths before editing.** If any file is on the forbidden
   list, stop.
2. **Do not write forbidden code in any form** — not as a patch, draft,
   pseudocode, test, or "example for the human to adapt". Offer a human-first
   plan instead.
3. **Do not expand scope.** Fix only what the human asked for, in the file(s)
   they named. Do not refactor neighboring forbidden code.
4. **Do not add dependencies** or change build architecture.
5. **Do not write commit messages or PR descriptions.** Humans author those.
6. **Show your diff and test results**; the human reviews and commits.
7. **Ask when unsure.** "Is this allowed?" defaults to no.

## If a request is forbidden

Reply with:

- The specific policy line that applies.
- A short, non-code plan the human can implement themselves.
- A pointer to the emergency override process in `.agents/workflows.md`
  (GitHub issue → maintainer approval → two-human review).

Do not implement first and ask for forgiveness later.

## Done when

- The change fits an allowed category.
- The matching skill's workflow was followed.
- A human has reviewed the diff and owns the commit.
