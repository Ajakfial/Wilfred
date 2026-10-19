---
name: project-orientation
description: Get oriented in the Wilfred repository. Use when you need the project layout, build and test commands, coding conventions, or help finding the right file before making an allowed change.
---

# Project Orientation

Wilfred is a fast, lightweight, cross-platform desktop search engine and
launcher written in C++20 with no third-party dependencies. This skill maps
the repository and the commands you need before touching it.

## Layout

| Path | Contents |
|------|----------|
| `include/wilfred/<module>/` | Public headers, one subdirectory per module |
| `src/<module>/` | Implementation, mirrors `include/wilfred/` |
| `src/platform/` | Windows / macOS / Linux / BSD backends behind `platform/native.hpp` |
| `src/search/`, `src/index/`, `src/query/` | **Forbidden for AI changes** |
| `src/ui/` | **Forbidden for AI changes** |
| `src/core/`, `src/config/` | In-tree JSON, YAML, HTTP, mmap, string pool |
| `config/wilfred.default.yml` | Shipped default configuration |
| `ui/overlay/` | HTML/CSS/JS for the search overlay |
| `tests/` | Unit tests, one file per module (`wilfred_tests` binary) |
| `benches/` | Microbenchmarks (`wilfred_bench` binary) |
| `scripts/` | Per-platform build and packaging scripts |
| `docs/` | User and contributor documentation |
| `lang/` | Translation catalogs (`en.yml` is the reference) |
| `.agents/` | AI policy, workflows, templates, scripts, and these skills |

Project-level docs worth opening first:

- `docs/architecture.md` — module tour and data flow.
- `docs/building.md` — full build options and CMake presets.
- `CONTRIBUTING.md` — conventions and review expectations.
- `docs/README.md` — documentation index.

## Build and test

Use the platform script; it configures, builds, and optionally runs `ctest`.

```bash
# Linux
./scripts/build-linux.sh --debug --tests

# macOS
./scripts/build-macos.sh --debug --tests

# BSD
./scripts/build-bsd.sh --tests
```

```powershell
# Windows (PowerShell)
./scripts/build-windows.ps1 -Config Debug -Tests
```

Build directories default to `build/linux`, `build/macos`, `build/windows`.
Manual equivalent:

```bash
cmake -S . -B build/local -DCMAKE_BUILD_TYPE=Debug -DWILFRED_BUILD_TESTS=ON
cmake --build build/local --config Debug
ctest --test-dir build/local --output-on-failure -C Debug
```

Useful CMake options and presets:

- `WILFRED_BUILD_TESTS` (default `ON`)
- `WILFRED_BUILD_BENCH` (default `ON`)
- `WILFRED_ENABLE_SANITIZERS` (default `OFF`, non-MSVC Debug)
- Presets: `windows-debug`, `linux-debug`, `macos-debug`, `freebsd-debug`
  (plus matching `-release`)

## Conventions

- **C++20.** No exceptions for control flow in hot paths; the index and search
  engines return status structs/bools.
- **No new third-party dependencies.** Dependency-free is a project goal.
- **Mirrored layout.** Headers in `include/wilfred/<module>/`, implementation
  in `src/<module>/`.
- **Formatting.** 2-space indent, same-line braces, `snake_case` functions and
  variables, `PascalCase` types. `.clang-format` and `.clang-tidy` are scoped
  to changed lines in CI.
- **Include order.** `.clang-format` sorts includes alphabetically. System
  headers that must come first (`windows.h`, `winsock2.h`, BSD sockets) go in
  their own blank-line-separated block.
- **Platform code.** Guard with `#ifdef _WIN32` / `__APPLE__` / else-Linux and
  keep it under `src/platform/`.
- **Config keys.** A new key requires `include/wilfred/config/config.hpp`, the
  loader in `src/config/config.cpp`, `config/wilfred.default.yml`, and
  `docs/configuration.md`.
- **Tests.** Lightweight harness in `tests/test.hpp`; no GoogleTest macros.

## Before editing

1. Load `wilfred-ai-policy`.
2. Run `git status` and read the target file end-to-end.
3. Confirm the change is in an allowed category.
4. Match the style of the surrounding code, not the style you prefer.
5. Keep the diff to the file(s) the human named.

## Guardrails

- Do not "explore by editing". Read first.
- Do not reformat whole files.
- Do not touch forbidden paths even for a drive-by fix; report the issue
  instead.
- If a build fails for reasons unrelated to the change, report it rather than
  fixing unrelated code.
