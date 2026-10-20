# GitHub Copilot Instructions

`AGENTS.md` is the authoritative AI policy for this repository. Read it before
suggesting or applying changes. The rules below are a summary.

## Allowed exceptions

Documentation, minor minis, small fixes, moderate refactors, test additions,
build/CI tweaks, and localization. See `.agents/allowed.md` for workflows.

## Forbidden

Core algorithms, platform internals, system integration, architecture
decisions, security-sensitive code, and performance-critical paths. See
`.agents/forbidden.md`. Do not produce forbidden code as a suggestion, draft,
pseudocode, example, or test.

## Review and commits

Every AI-assisted change needs line-by-line human review. Do not generate
commit messages or PR descriptions. PR descriptions must include
`Human-authored: yes`.

`.copilotignore` mirrors `.cursorignore` where the Copilot surface supports
it. Repository-level content exclusions are configured in GitHub settings, so
that file is advisory only.
