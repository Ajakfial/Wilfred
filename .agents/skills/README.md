# .agents/skills — Project Skills for AI Agents

This directory contains task-scoped instruction packs ("skills") that an AI
coding agent can load on demand. Each skill lives in its own directory and is
defined by a `SKILL.md` file with YAML frontmatter.

The skills encode **how to do the work the policy already allows** — they do
not expand what AI may do. Read `AGENTS.md` first.

## Available skills

| Skill | Use when |
|-------|----------|
| [`wilfred-ai-policy`](wilfred-ai-policy/SKILL.md) | **Start here.** Any task, before the first edit. |
| [`project-orientation`](project-orientation/SKILL.md) | You need the layout, build commands, or conventions. |
| [`documentation`](documentation/SKILL.md) | Editing `docs/`, `README.md`, or any `*.md` file. |
| [`tests`](tests/SKILL.md) | Adding or extending a unit test under `tests/`. |
| [`minor-minis`](minor-minis/SKILL.md) | A small addition to `src/search/minis.cpp` from a human-written skeleton. |
| [`localization`](localization/SKILL.md) | Updating `lang/*.yml` or translation keys. |
| [`build-and-ci`](build-and-ci/SKILL.md) | Small `.github/workflows/` or CMake changes. |
| [`commit-and-pr`](commit-and-pr/SKILL.md) | Preparing a change for review or commit. |

## How agents discover these

* Agents that understand the `SKILL.md` convention can read this directory
  directly.
* **Claude Code** reads project skills from `.claude/skills/`. Link this
  directory there once (the link is git-ignored; this directory stays the
  source of truth):

  ```powershell
  # Windows (PowerShell), from the repo root
  New-Item -ItemType Directory -Force .claude | Out-Null
  New-Item -ItemType Junction -Path .claude\skills -Target (Resolve-Path .agents\skills)
  ```

  ```bash
  # macOS / Linux, from the repo root
  mkdir -p .claude
  ln -s ../.agents/skills .claude/skills
  ```

* Other tools: point their project-instructions config at this directory, or
  mention the relevant `SKILL.md` path in the prompt.

`AGENTS.md` also points here, so agents that only read `AGENTS.md` still find
the skills.

## Skill format

```markdown
---
name: example-skill
description: When the agent should load this skill. One or two sentences.
---

# Example Skill

## Use when
...

## Steps
...

## Guardrails
...

## Done when
...
```

Rules for new skills:

1. Directory name and frontmatter `name` must match, lowercase with hyphens.
2. `description` must state the trigger, not just the topic. Keep it under
   1024 characters.
3. Scope the skill to one task and to work allowed by `AGENTS.md`.
4. Reference real files and commands; do not invent flags or paths.
5. Tell the agent when to **stop and ask a human**.
6. Add the skill to the table above.

## Guardrail

If a skill and `AGENTS.md` ever disagree, `AGENTS.md` wins. Skills must never
be used to justify AI work on search, ranking, indexing, platform internals,
system integration, security, performance-critical paths, architecture, or
dependencies.
