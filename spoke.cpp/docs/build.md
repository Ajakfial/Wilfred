# Build guide -- Windows, Linux, macOS

Spoke is plain C++17 with no third-party dependencies. The same tree builds
on all three desktop OSes; CI proves it on every push
(`.github/workflows/ci.yml`: `windows-latest`, `ubuntu-latest`, `macos-latest`).

## Requirements

| OS | Compiler | Install |
|---|---|---|
| Windows | MSVC 19.30+ (VS 2022 / BuildTools) | `winget install Kitware.CMake` |
| Linux | GCC 11+ or Clang 14+ | `sudo apt install cmake ninja-build pkg-config g++` (or `dnf`/`pacman` equivalents) |
| macOS (Intel + Apple Silicon) | AppleClang (Xcode CLT) | `xcode-select --install`, `brew install cmake ninja pkg-config` |

- CMake ≥ 3.20, .NET 8+ SDK **only** for the C#/MAUI hosts (engine needs none).
- Threads/pthreads ship with every OS. No vcpkg/conan, no downloads.

## One-command builds

```bash
# Linux
bash scripts/build-linux.sh                 # Release, auto threads
bash scripts/build-linux.sh --debug         # Debug
bash scripts/build-linux.sh --no-native     # portable binary (no -march=native)
bash scripts/build-linux.sh --install-prefix=/usr/local

# macOS
bash scripts/build-macos.sh [--debug] [--install-prefix DIR]

# Windows (PowerShell)
powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1
```

## CMake presets (equivalent, IDE-friendly)

```
windows-vs2022      Visual Studio 2026/2022, x64      (multi-config: --config Release)
linux-ninja-release Ninja, CMAKE_BUILD_TYPE=Release
linux-ninja-debug   Ninja, Debug
macos-ninja-release Ninja, Release (-march=native OFF: AppleClang-safe)
macos-ninja-debug   Ninja, Debug
```

```bash
cmake --preset linux-ninja-release
cmake --build --preset linux-ninja-release --parallel
ctest --test-dir build/linux-ninja-release --output-on-failure
./build/linux-ninja-release/spoke_cli info
```

List them any time: `cmake --list-presets`.

## Manual builds (no presets)

```powershell
# Windows
cmake -S . -B build
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
.\build\Release\spoke_cli.exe info
```

```bash
# Linux / macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/spoke_cli transcribe speech.wav
```

## Install & packaging

```bash
cmake --install build/linux-ninja-release
# Layout: <prefix>/bin/spoke_cli, <prefix>/lib/libspoke.so*, <prefix>/include/spoke/, <prefix>/lib/pkgconfig/spoke.pc
# The presets configure a local prefix (build/<preset>/install). To relocate,
# reconfigure with -DCMAKE_INSTALL_PREFIX=... rather than overriding --prefix
# at install time (keeps spoke.pc paths exact). For packaging/staging use:
#   DESTDIR=/tmp/stage cmake --install build/linux-ninja-release --prefix /usr/local
```

- `pkg-config --cflags --libs spoke` works after install (`packaging/spoke.pc.in`).
- CPack targets: `TGZ` everywhere, `DEB` on Linux, `DragNDrop` on macOS, `ZIP` on Windows:
  `cpack --preset linux-ninja-release` (or `cpack` inside the build dir).
- Shared-library versioning: `libspoke.so.0` / `libspoke.0.dylib` (SOVERSION 0)
  with unversioned `libspoke.so` / `libspoke.dylib` + `spoke.dll` symlinks for linking.

## Native libraries per OS

| OS | Engine library | CLI binary | .NET host loads via |
|---|---|---|---|
| Windows | `spoke.dll` | `spoke_cli.exe` | app dir / `SPOKE_LIBRARY_PATH` / `--lib` |
| Linux | `libspoke.so[.0]` | `spoke_cli` | app dir / `SPOKE_LIBRARY_PATH` / `--lib` / ldconfig |
| macOS | `libspoke.dylib` | `spoke_cli` | app bundle / `SPOKE_LIBRARY_PATH` / `--lib` / `@rpath` |

`spoke_cli info` prints the platform triple, e.g. `linux (x86_64, GCC 13.2)`
or `macos (arm64, Clang 15.0 (Apple))`, plus the expected library filename --
use it to confirm you built the right artifact.

## CMake options

| Option | Default | Effect |
|---|---|---|
| `SPOKE_BUILD_TESTS` | ON | `spoke_tests` + `ctest` registration |
| `SPOKE_BUILD_BENCH` | ON | `spoke_bench` harness |
| `SPOKE_BUILD_CLI` | ON | `spoke_cli` binary |
| `SPOKE_BUILD_SHARED` | ON | shared lib for .NET P/Invoke |
| `SPOKE_BUILD_EXAMPLES` | OFF | `spoke_streaming_stdin` demo |
| `SPOKE_ENABLE_MARCH_NATIVE` | ON | `-march=native` where safe (auto-OFF on Apple, MSVC, cross-compiles) |

MSVC gets `/O2 /Oi /Ot`; GCC/Clang get `-O3 -ffast-math -funroll-loops`
(+ `-Wshadow`). The shared lib uses hidden visibility with explicit
`SPOKE_API` exports, and is position-independent for clean P/Invoke loading.

## .NET hosts

- `ui/SpokeConsole` (`net8.0`, runs on all three OSes):
  `dotnet run --project ui/SpokeConsole -- info --lib ./build/linux-ninja-release`
- `ui/SpokeApp` (MAUI: Windows / MacCatalyst / iOS / Android -- MAUI has no
  Linux target, which is exactly why the console host exists).
- Both share the same `Services/` sources; `SpokeNativeService` probes
  `--lib DIR` → `SPOKE_LIBRARY_PATH` → app directory → OS loader, trying
  `spoke.dll` / `libspoke.so[.0]` / `libspoke.dylib` as appropriate.

## Troubleshooting

- `cmake not found` → install per the table above; CI uses the same commands.
- `ninja not found` (Linux/macOS) → `apt install ninja-build` / `brew install ninja`,
  or drop `--preset` and use the manual `Unix Makefiles` default.
- Old distro compiler (GCC < 11) → install `gcc-12`/`clang-14` and pass
  `-DCMAKE_CXX_COMPILER=clang++-14` with `-DSPOKE_ENABLE_MARCH_NATIVE=OFF`.
- `cannot find -lspoke` when linking your own app → `export PKG_CONFIG_PATH=~/.local/lib/pkgconfig`
  or pass `-DCMAKE_PREFIX_PATH=~/.local`.
- macOS `library not loaded: libspoke.dylib` → the build sets `@loader_path`
  rpaths; keep the dylib next to the consumer or set `SPOKE_LIBRARY_PATH`.
- Windows `spoke.dll not found` at .NET runtime → copy it next to the app exe
  or pass `--lib build\windows-vs2022\Release`.
