# Repository Layout

What lives at the top level, and why. The module tour is in
[architecture.md](architecture.md); this page only covers where files sit.

## Root directories

| Path | Contents |
|---|---|
| `include/`, `src/` | Public headers and implementation, mirrored per module |
| `tests/`, `benches/` | Unit tests and microbenchmarks |
| `config/` | Shipped default configuration |
| `cmake/` | CMake helpers |
| `scripts/`, `packaging/` | Build, packaging, and release tooling |
| `lang/` | UI translation catalogs (`en.yml` is the reference) |
| `docs/` | Documentation; start at [README.md](README.md) |
| `ui/`, `www/`, `android/`, `ios/` | Overlay and platform clients |
| `spoke.cpp/` | Vendored in-process transcription engine (built by CMake) |
| `.agents/` | AI policy, workflows, skills, and the tool ignore lists |
| `.github/` | Workflows, templates, and Copilot instructions |

## Root files

* `CMakeLists.txt`, `CMakePresets.json`, `codecov.yml` -- build and coverage.
* `AGENTS.md` -- the canonical AI policy. `CLAUDE.md` and `GEMINI.md` are thin
  pointers to it, kept at the root because each tool auto-loads its own name.
* `README.md` -- quick start and the English source of truth.
* `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, `AUTHORS.md`,
  `PHILOSOPHY.md`, `ROADMAP.md`, `SHAME.md`, `LICENSE` -- project policy and
  community documents, left at the root so GitHub and contributors find them.
* `.gitignore`, `.git-blame-ignore-revs` -- git configuration.

## Translated READMEs

The `README_<code>.md` files live in [i18n/](i18n/README.md) rather than the
root. The English [README](../README.md) is canonical; translations may lag.
UI strings are separate and live in `lang/<code>.yml` -- see
[localization.md](localization.md).

## Tool ignore lists

The per-tool ignore lists (`.cursorignore`, `.geminiignore`, `.aiderignore`,
`.codeiumignore`, `.aiexclude`, `.clineignore`, `.rooignore`,
`.augmentignore`, `.copilotignore`, `.antigravityignore`, and `.sccignore` for
the `scc` code counter) live in `.agents/ai-ignore/`. They mirror
`.agents/ai-ignore/.cursorignore`.

AI tools only look for these files in the repository root, so in this checkout
they are dormant. Copy the ones you need back before an AI session:

```bash
cp .agents/ai-ignore/.[a-z]*ignore .
cp .agents/ai-ignore/.aiexclude .
```

The instruction files (`AGENTS.md`, `CLAUDE.md`, `GEMINI.md`,
`.github/copilot-instructions.md`) stay at the root for the same
auto-discovery reason. See [`.agents/README.md`](../.agents/README.md) for the
per-tool table and the PowerShell equivalent.

## Adding a new translated README

```bash
cp README.md docs/i18n/README_<code>.md
```

Translate the prose, leave code blocks and paths unchanged, keep the line 1
notice, and add a row to [i18n/README.md](i18n/README.md).

## Adding a new tool ignore list

Create `.<tool>ignore` in `.agents/ai-ignore/` from the
`.cursorignore` pattern list, then add a row to
[`.agents/README.md`](../.agents/README.md). Do not put it at the root: the
root stays limited to files that the tools or git must discover there.
