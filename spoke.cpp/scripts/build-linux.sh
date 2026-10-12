#!/usr/bin/env bash
# One-command build + test for Linux (Ubuntu/Debian/Fedora/Arch).
# Usage: bash scripts/build-linux.sh [--debug] [--no-native] [--install-prefix DIR]
set -euo pipefail

PRESET="linux-ninja-release"
NATIVE_FLAG=""
PREFIX=""

for arg in "$@"; do
  case "$arg" in
    --debug) PRESET="linux-ninja-debug" ;;
    --no-native) NATIVE_FLAG="-DSPOKE_ENABLE_MARCH_NATIVE=OFF" ;;
    --install-prefix=*) PREFIX="${arg#*=}" ;;
    -h|--help)
      echo "Usage: bash scripts/build-linux.sh [--debug] [--no-native] [--install-prefix=DIR]";
      echo "  Needs: cmake >= 3.20, ninja, gcc >= 11 or clang >= 14 (see docs/build.md).";
      exit 0 ;;
    *) echo "Unknown arg: $arg (try --help)" >&2; exit 1 ;;
  esac
done

command -v cmake >/dev/null || { echo "error: cmake not found (apt install cmake)" >&2; exit 1; }
command -v ninja >/dev/null || { echo "error: ninja not found (apt install ninja-build)" >&2; exit 1; }

EXTRA=()
[[ -n "$NATIVE_FLAG" ]] && EXTRA+=("$NATIVE_FLAG")
[[ -n "$PREFIX" ]] && EXTRA+=("-DCMAKE_INSTALL_PREFIX=$PREFIX")

echo "==> configure ($PRESET)"
cmake --preset "$PRESET" "${EXTRA[@]:-}"
echo "==> build"
cmake --build --preset "$PRESET" --parallel
echo "==> test"
ctest --test-dir "build/$PRESET" --output-on-failure
echo "==> info"
"./build/$PRESET/spoke_cli" info
echo "OK: Linux build green. Library: build/$PRESET/libspoke.so"
