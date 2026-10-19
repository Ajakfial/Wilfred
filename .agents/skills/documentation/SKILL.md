---
name: documentation
description: Write or edit Wilfred documentation from a human-provided outline. Use for changes under docs/, README.md, CONTRIBUTING.md, or any other markdown file, including cross-references and tables of contents.
---

# Documentation

Documentation is one of the explicitly allowed AI uses. The human supplies the
technical content; you turn it into clear prose that matches the existing docs.

## Preconditions

- A human has written an outline, bullets, or a precise description of the
  change. If not, ask for one.
- The facts are verifiable in the repository (source, config, scripts, tests).
  Do not invent features, flags, defaults, or file paths.

## Workflow

1. Read the target file and one or two neighboring docs to match voice and
   structure.
2. Read the source of truth for every claim:
   - CLI flags → `src/main.cpp`, `docs/cli.md`
   - Config keys → `config/wilfred.default.yml`, `src/config/config.cpp`
   - Build options → `CMakeLists.txt`, `CMakePresets.json`
   - Locale keys → `src/locale/locale.cpp`
3. Expand the human's outline. Keep their technical details exactly.
4. Add cross-references only to files that exist.
5. Check headings, tables, code fences, and links render correctly.
6. Hand the diff back for word-by-word human review.

## Style

- Start each page with a single `# Title`; use `##` for sections.
- Open with one or two sentences explaining what the page covers.
- Use `*` for bullet lists (matches most existing docs).
- Use fenced code blocks with a language tag (`bash`, `cpp`, `yaml`, `json`).
- Prefer present tense and active voice.
- Use relative links between docs, e.g. `[building.md](building.md)`.
- Keep line length close to the surrounding file (most docs wrap near 80
  columns).
- Preserve `{placeholders}`, command syntax, and config key spelling exactly.
- Document behavior that exists today; mark planned work explicitly if asked.

## Do not

- Write a page from scratch without a human outline.
- Invent options, defaults, platforms, or output examples.
- Rewrite unrelated sections or reformat the whole file.
- Change code while documenting it.
- Touch the "made by Cursor AI" notice in `README.md` (line 1) unless the
  human explicitly asks; it is a project-owner decision.
- Generate commit messages or PR descriptions.

## Verify

- Every command in a code block is runnable as written.
- Every link resolves to an existing path or anchor.
- Every table has a header separator row.
- No claim contradicts `AGENTS.md`, `CONTRIBUTING.md`, or the source.

## Done when

- The prose covers exactly the human's outline.
- A human has reviewed every word for technical accuracy.
