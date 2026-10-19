# AGENTS.md -- AI Usage Policy

## Allowed AI Use (No Restriction)

You MAY use AI freely for:

- **Documentation**: Writing, editing, formatting, translating docs (`docs/`, `README.md`, `*.md`)
- **Minor minis**: Adding/fixing small mini commands (`src/search/minis.cpp` -- trivial additions, typo fixes, help text)
- **Small fixes**: One-liner bug fixes, typo corrections, comment updates
- **Moderate refactors**: Code style, naming, dead code removal, simple logic simplification
- **Test additions**: Adding unit tests for existing functionality
- **Build/CI tweaks**: `.github/workflows/`, `CMakeLists.txt`, `CMakePresets.json`
- **Localization**: Adding/updating translation strings (`lang/*.yml`)

## Forbidden AI Use (Immediate Ban + Public Shame)

You are PERMANENTLY BANNED from this repository and will be PUBLICLY SHAMED if you use AI for:

- **Core algorithms**: Search engine, ranking, fuzzy matching, index structures (`src/search/`, `src/index/`)
- **Platform internals**: Native backends, watchers, hotkeys, overlays (`src/platform/`, `src/ui/`)
- **System integration**: Autostart, power actions, toggles, window management, media keys
- **Architecture decisions**: Adding/removing modules, changing data structures, new dependencies
- **Security-sensitive code**: Auth, crypto, sandboxing, privilege escalation
- **Performance-critical paths**: Hot loops, memory allocators, SIMD, lock-free code
- **Generated code commits**: Committing AI-written code without thorough human review
- **Pull requests authored by AI**: PRs where the description/code was AI-generated

## Enforcement

- **First offense**: Permanent ban from the repository (no appeals)
- **Public shaming**: Your GitHub handle will be added to `SHAME.md` with the offending commit/PR link
- **No exceptions**: "I only used AI a little" is not a defense

## Human Review Requirement

Even for allowed uses, ALL AI-assisted changes must pass human code review. The human author is fully responsible for correctness, style, and security.

## Project Skills

Task-scoped agent skills live in `.agents/skills/`. Start with
`.agents/skills/wilfred-ai-policy/SKILL.md`; it restates this policy and points
to the workflow for each allowed task. Skills never expand what AI may do. See
`.agents/skills/README.md` for the format and for linking the directory into
`.claude/skills/` for Claude Code.

---

*This policy exists because Wilfred is a human-crafted project. AI is a tool, not a contributor.*