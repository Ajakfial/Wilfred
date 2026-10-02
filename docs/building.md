# Building Wilfred

Wilfred builds with plain CMake and has no third-party library dependencies
to fetch — everything under `src/core/` and `src/config/` (JSON, YAML, CRC32,
mmap wrapper, etc.) is implemented in-tree.

## Requirements

* **CMake 3.16+**
* **A C++20 compiler**: MSVC (Windows), Apple Clang (macOS), or GCC/Clang
  (Linux)
* **Linux only**: X11 development headers for the overlay window
  (`libx11-dev` on Debian/Ubuntu, `libX11-devel` on Fedora)
* Optional: **Ninja**, used automatically by the build scripts if present
  (falls back to Makefiles on Linux, or the default Visual Studio/Xcode
  generator otherwise)

## Quickest path: the build scripts

Each platform has a script under `scripts/` that configures, builds, and
optionally tests in one step:

```bash
# Linux
./scripts/build-linux.sh [options]

# macOS
./scripts/build-macos.sh [options]
```

```powershell
# Windows (run from a PowerShell prompt; the script imports the MSVC
# developer environment for you if it isn't already active)
./scripts/build-windows.ps1 [options]
```

```cmd
:: Windows (cmd.exe wrapper around the PowerShell script)
scripts\build-windows.cmd [options]
```

Common options (Linux/macOS; Windows uses PowerShell parameter names shown
in brackets):

| Option | PowerShell equivalent | Effect |
|---|---|---|
| `--debug` / `--release` | `-Config Debug` / `-Config Release` | Build type (default Release) |
| `--build-dir DIR` | `-BuildDir DIR` | CMake binary directory (default `build/<platform>`) |
| `--jobs N` | `-Jobs N` | Parallel compile jobs |
| `--tests` | `-Tests` | Run `ctest` after building |
| `--sanitizers` | *(Linux/macOS only)* | Enable ASan/UBSan (non-MSVC) |
| `--clean` | `-Clean` | Remove the build directory first |
| `--generator NAME` | `-Generator NAME` | Override the CMake generator |
| `--deps` | *(Linux/macOS only)* | Install system build dependencies (`apt-get` / `brew`) |
| `--arch ARCH` | `-Arch ARCH` | macOS: `CMAKE_OSX_ARCHITECTURES` (`arm64`, `x86_64`, or both). Windows: `x86`/`x64`/`arm64` |
| `-- <args>` | `<args>` (positional) | Anything after `--` (or extra positional args) is forwarded to CMake's configure step |

Environment variable overrides (Linux/macOS scripts): `WILFRED_CONFIG`,
`WILFRED_BUILD_DIR`, `WILFRED_JOBS`, and on macOS `WILFRED_ARCH`.

Example — a debug build with tests and sanitizers on Linux:

```bash
./scripts/build-linux.sh --debug --tests --sanitizers
```

The resulting binaries land in `<build-dir>/wilfred[.exe]`,
`<build-dir>/wilfred_tests[.exe]`, and `<build-dir>/wilfred_bench[.exe]`
(exact subpath varies slightly by generator — Visual Studio/Xcode
multi-config generators nest a `<Config>/` folder).

## Manual CMake invocation

If you'd rather not use the scripts:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Useful cache variables (all boolean, `ON`/`OFF`):

| Variable | Default | Effect |
|---|---|---|
| `WILFRED_BUILD_TESTS` | `ON` | Build the `wilfred_tests` target |
| `WILFRED_BUILD_BENCH` | `ON` | Build the `wilfred_bench` target |
| `WILFRED_ENABLE_SANITIZERS` | `OFF` | Add `-fsanitize=address,undefined` (non-MSVC only) |

To skip tests/benchmarks for a faster configure:

```bash
cmake -S . -B build -DWILFRED_BUILD_TESTS=OFF -DWILFRED_BUILD_BENCH=OFF
```

## CMake presets

`CMakePresets.json` (schema version 6) defines per-platform presets, each
gated with a `hostSystemName` condition so only the matching platform's
presets are usable on a given machine:

| Preset | Config |
|---|---|
| `windows-release` / `windows-debug` | Ninja, x64 |
| `linux-release` / `linux-debug` | Ninja (falls back to Makefiles) |
| `macos-release` / `macos-debug` | Ninja/Xcode |

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug   # if a matching test preset exists in your CMake version
```

All presets build to `build/<preset-name>/` and enable
`WILFRED_BUILD_TESTS`/`WILFRED_BUILD_BENCH` by default (sanitizers off).

## Running the binary after building

```bash
# from the build directory, or add it to PATH
./build/linux/wilfred            # run the daemon
./build/linux/wilfred search vs  # one-shot search
```

On first run, Wilfred writes a default config to the platform config
directory (see [configuration.md](configuration.md)) — no manual setup step
is required before first launch.

## Running tests and benchmarks

```bash
cmake --build build --target wilfred_tests --config Release
ctest --test-dir build --output-on-failure -C Release

cmake --build build --target wilfred_bench --config Release
./build/wilfred_bench
```

`tests/` has one file per module (`test_index.cpp`, `test_rank.cpp`,
`test_config.cpp`, `test_fuzzy.cpp`, `test_filter.cpp`, `test_classify.cpp`,
`test_apps.cpp`, `test_math.cpp`, `test_yaml.cpp`, `test_tokenizer.cpp`,
`test_history.cpp`, `test_paths.cpp`, `test_protocol.cpp`,
`test_extensibility.cpp`, `test_search_extras.cpp`, `test_main.cpp`) using
the minimal harness in `tests/test.hpp`. `benches/bench_main.cpp` measures
intern/insert/query/fuzzy/ranking/filter/math/snapshot I/O throughput across
1K–100K synthetic records.

## Packaging (what CI does)

The `.github/workflows/release.yml` workflow builds `wilfred` for
Windows/Linux/macOS whenever a `v*.*.*` tag is pushed, then packages each
platform's binary alongside `ui/overlay/`, `README.md`, and
`config/wilfred.default.yml` into a zip/tar.gz and attaches it to a GitHub
Release. Windows additionally produces a signed `.msi` installer from the
same staging dir (see [installer.md](installer.md)). On Windows it also stamps Publisher `Wilfred Open Contributors`
via VERSIONINFO on every build and Authenticode-signs when signing secrets
are present — see [signing.md](signing.md). There is currently no CI job
that runs on pull requests — run `ctest` locally before submitting a change (see
[CONTRIBUTING.md](../CONTRIBUTING.md)).

## Troubleshooting

* **Linux: overlay fails to build / link errors mentioning X11** — install
  `libx11-dev` (or run the build script with `--deps`).
* **Windows: `VsDevCmd.bat not found`** — install the "Desktop development
  with C++" workload in Visual Studio Build Tools; `build-windows.ps1` looks
  it up via `vswhere.exe`.
* **Windows: running `wilfred.exe` directly shows no console output** — the
  daemon build is a GUI subsystem app (`wWinMain`); it re-attaches to a
  parent console for CLI subcommands like `search`/`status`, but has no
  console of its own when double-clicked. Use `wilfred daemon` from an
  existing terminal, or check the tray icon to quit.
