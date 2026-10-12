#!/usr/bin/env bash
# One-command build + test for macOS (Intel + Apple Silicon).
# Usage: bash scripts/build-macos.sh [--debug] [--install-prefix DIR]
# Needs: Xcode Command Line Tools (xcode-select --install), cmake, ninja:
#   brew install cmake ninja pkg-config
set -euo pipefail

PRESET="macos-ninja-release"
PREFIX=""

for arg in "$@"; do
  case "$arg" in
    --debug) PRESET="macos-ninja-debug" ;;
    --install-prefix=*) PREFIX="${arg#*=}" ;;
    -h|--help)
      echo "Usage: bash scripts/build-macos.sh [--debug] [--install-prefix DIR]";
      exit 0 ;;
    *) echo "Unknown arg: $arg (try --help)" >&2; exit 1 ;;
  esac
done

command -v cmake >/dev/null || { echo "error: cmake not found (brew install cmake)" >&2; exit 1; }
command -v ninja >/dev/null || { echo "error: ninja not found (brew install ninja)" >&2; exit 1; }
xcode-select -p >/dev/null 2>&1 || { echo "error: Xcode CLT missing (xcode-select --install)" >&2; exit 1; }

EXTRA=()
[[ -n "$PREFIX" ]] && EXTRA+=("-DCMAKE_INSTALL_PREFIX=$PREFIX")

echo "==> configure ($PRESET) [$(uname -m)]"
cmake --preset "$PRESET" "${EXTRA[@]:-}"
echo "==> build"
cmake --build --preset "$PRESET" --parallel
echo "==> test"
ctest --test-dir "build/$PRESET" --output-on-failure
echo "==> info"
"./build/$PRESET/spoke_cli" info
echo "OK: macOS build green. Library: build/$PRESET/libspoke.dylib"
