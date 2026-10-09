# Contributing to Wilfred

Thanks for taking the time to contribute. Wilfred is a small, dependency-light
C++20 codebase, and contributions of all sizes are welcome: bug reports,
documentation fixes, new search providers, ranking tweaks, platform-specific
fixes, and new features.

This guide covers how to get the project building, the conventions the
codebase follows, and how to submit a change.

## Code of conduct

Participation in this project is governed by the
[Code of Conduct](CODE_OF_CONDUCT.md). By participating, you agree to abide
by it.

## Reporting bugs

Before opening an issue, please check whether it's already reported. A good
bug report includes:

* Your OS and version (Windows build, macOS version, or Linux distro).
* The output of `wilfred status` and, if relevant, `wilfred --help`.
* Your `wilfred.yml` if the issue is config-related (redact anything
  sensitive — the `sync.token` and `api.token` fields in particular).
* Steps to reproduce, and what you expected vs. what happened.
* Whether the daemon, the CLI (`wilfred search ...`), or the overlay UI is
  involved.

## Ways to contribute

* **Bug reports** — see [Reporting bugs](#reporting-bugs) below.
* **Bug fixes and small improvements** — always welcome, open a PR directly.
* **New features** — for anything non-trivial (a new query type, a new mini,
  a new platform backend, a change to the on-disk index format), please open
  an issue first to discuss the design. This avoids wasted work if the
  approach needs to change.
* **Documentation** — fixes and additions to `README.md` and `docs/` are
  welcome and don't require a design discussion.
* **Security issues** — do **not** open a public issue. See
  [SECURITY.md](SECURITY.md).

## Project layout

See [docs/architecture.md](docs/architecture.md) for a full tour. In short:

| Path | Contents |
|---|---|
| `include/wilfred/` | Public headers, one subdirectory per module |
| `src/` | Implementation, mirrors `include/wilfred/` |
| `src/platform/` | Windows / macOS / Linux backends behind `platform/native.hpp` |
| `config/wilfred.default.yml` | The shipped default configuration |
| `ui/overlay/` | HTML/CSS/JS for the search overlay window |
| `tests/` | Unit tests (one file per module, `wilfred_tests` binary) |
| `benches/` | Microbenchmarks (`wilfred_bench` binary) |
| `scripts/` | Per-platform build scripts used locally and in CI |

Platform-specific code lives only under `src/platform/` and behind the
interface declared in `include/wilfred/platform/native.hpp` and
`platform.hpp`. Everything else in `src/` and `include/` is expected to be
portable C++20 and must compile on Windows (MSVC), macOS (Apple Clang), and
Linux (GCC/Clang).

## Building

Requirements: CMake 3.16+, a C++20 compiler, and (on Linux) X11 development
headers for the overlay (`libx11-dev` / `libX11-devel`).

```bash
# Linux
./scripts/build-linux.sh --debug --tests

# macOS
./scripts/build-macos.sh --debug --tests

# Windows (PowerShell)
./scripts/build-windows.ps1 -Config Debug -Tests
```

Each script accepts `--clean`, `--jobs N`, and forwards any arguments after
`--` straight to `cmake`'s configure step. See
[docs/building.md](docs/building.md) for the full option list, CMake presets,
and manual (non-script) build steps.

Two CMake options control optional targets:

* `WILFRED_BUILD_TESTS` (default `ON`) — builds `wilfred_tests`
* `WILFRED_BUILD_BENCH` (default `ON`) — builds `wilfred_bench`
* `WILFRED_ENABLE_SANITIZERS` (default `OFF`) — enables ASan/UBSan on
  non-MSVC Debug builds

## Running the test suite

```bash
cmake --build build/linux --target wilfred_tests --config Debug
ctest --test-dir build/linux --output-on-failure
```

(swap `build/linux` for your build directory). Please add or extend a test
under `tests/` for any behavioral change — see the existing `tests/test_*.cpp`
files for the lightweight test harness (`tests/test.hpp`). There is one test
file per module (`test_index.cpp`, `test_rank.cpp`, `test_config.cpp`, etc.);
add new cases to the matching file, or create a new one and add it to the
`WILFRED_TEST_SOURCES` list in `CMakeLists.txt` if you're covering a new
module.

Note that `.github/workflows/release.yml` builds release binaries on tagged
pushes but does not run the test suite on pull requests. PRs do get a
`clang-format`/`clang-tidy` check on changed files
(`.github/workflows/lint.yml`), but please run `ctest` locally before
opening a PR — see ROADMAP.md for the plan to add it to CI too.

## Coding conventions

* **C++20**, no exceptions used for control flow in hot paths (the index and
  search engines return status structs / bools rather than throwing;
  `main.cpp` is the only place that catches `std::exception` at the top
  level).
* **No new third-party dependencies** without discussion first. Wilfred
  intentionally has none beyond the OS and a small vendored YAML/JSON/HTTP
  layer under `src/core` and `src/config`; keeping it dependency-free is a
  project goal (see `README.md`'s **LOW-RESOURCE** claim).
* **Headers** under `include/wilfred/<module>/` declare the public surface;
  implementation lives in the matching `src/<module>/`. Keep this mirrored
  layout when adding a module.
* **Formatting**: match the existing style in the file you're editing
  (2-space indent, braces on the same line, `snake_case` for functions and
  variables, `PascalCase` for types), enforced by `.clang-format` and
  `.clang-tidy`. If you land a formatting-only commit (no logic change),
  add its hash to `.git-blame-ignore-revs` in the same PR so `git blame`
  keeps pointing at the commit that actually changed behavior.
* **Include order**: `.clang-format` sorts `#include`s alphabetically, which
  breaks order-dependent system headers. `windows.h` (and `winsock2.h`
  where used) must come first, and followers such as `shellapi.h`,
  `WebView2.h`, or BSD socket headers keep their dependency order — put
  each in its own blank-line-separated block so the sorter can never
  reorder them (see `src/ui/win_overlay.cpp`, `src/search/minis.cpp`).
* **Warnings**: the build treats warnings as informative, not fatal, but PRs
  that introduce new `-Wall -Wextra -Wpedantic` (or `/W4`) warnings will be
  asked to fix them.
* **Platform code**: guard with `#ifdef _WIN32` / `__APPLE__` / else-Linux
  as done throughout `src/platform/` and `src/fs/watcher.cpp` — don't
  introduce a new platform abstraction pattern without discussion.
* **Config changes**: if you add a new config key, update all three of:
  `include/wilfred/config/config.hpp` (the `Config` struct), the YAML
  loader in `src/config/config.cpp`, and `config/wilfred.default.yml`, plus
  the reference table in `docs/configuration.md`.
* **Ranking changes**: new ranking signals should be added as a named
  `RankPipeline` signal (see `include/wilfred/search/rank.hpp`) rather than
  hard-coded into `rank_record`, unless they are a core signal every result
  needs.

## Commit and PR guidelines

* Keep PRs focused — one logical change per PR is much easier to review than
  a bundle of unrelated fixes.
* Write a clear PR description: what changed, why, and how you tested it
  (which platform(s) you built and ran on).
* Reference any related issue (`Fixes #123`).
* Make sure `ctest` passes locally on at least one platform before
  requesting review; note in the PR description which platform(s) you were
  able to test on, since reviewers may not be able to test all three.
* Be willing to iterate — review feedback is part of the process, not a
  rejection.

## License

By contributing, you agree that your contributions will be licensed under
the [MIT License](LICENSE) that covers the project.
