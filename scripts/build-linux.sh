#!/usr/bin/env bash
# Build Wilfred on Linux (CMake + GCC/Clang).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CONFIG="${WILFRED_CONFIG:-Release}"
BUILD_DIR="${WILFRED_BUILD_DIR:-build/linux}"
JOBS="${WILFRED_JOBS:-}"
SANITIZERS=OFF
RUN_TESTS=0
INSTALL_DEPS=0
CLEAN=0
GENERATOR=""
EXTRA_CMAKE=()

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

  --debug              Debug build
  --release            Release build (default)
  --build-dir DIR      CMake binary dir (default: build/linux)
  --jobs N             Parallel compile jobs
  --tests              Run ctest after the build
  --sanitizers         ASan/UBSan (non-MSVC Debug-friendly)
  --deps               Install apt packages: cmake, g++, ninja, libx11-dev
  --clean              Remove the build directory first
  --generator NAME     CMake generator (Ninja or Unix Makefiles)
  -h, --help           Show this help

Any extra arguments after -- are passed to cmake configure.
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --debug) CONFIG=Debug; shift ;;
    --release) CONFIG=Release; shift ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --tests) RUN_TESTS=1; shift ;;
    --sanitizers) SANITIZERS=ON; shift ;;
    --deps) INSTALL_DEPS=1; shift ;;
    --clean) CLEAN=1; shift ;;
    --generator) GENERATOR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    --) shift; EXTRA_CMAKE+=("$@"); break ;;
    *) EXTRA_CMAKE+=("$1"); shift ;;
  esac
done

if [[ "$(uname -s)" != "Linux" ]]; then
  echo "build-linux.sh must run on Linux." >&2
  exit 1
fi

if [[ "$INSTALL_DEPS" -eq 1 ]]; then
  if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update
    sudo apt-get install -y cmake g++ ninja-build pkg-config libx11-dev
  else
    echo "No apt-get found. Install cmake, a C++20 compiler, ninja, and libx11-dev." >&2
    exit 1
  fi
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is required (3.16+)." >&2
  exit 1
fi

if [[ -z "$GENERATOR" ]]; then
  if command -v ninja >/dev/null 2>&1; then
    GENERATOR=Ninja
  else
    GENERATOR="Unix Makefiles"
  fi
fi

if [[ "$CLEAN" -eq 1 && -d "$BUILD_DIR" ]]; then
  rm -rf "$BUILD_DIR"
fi

cmake -S "$ROOT" -B "$BUILD_DIR" -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$CONFIG" \
  -DWILFRED_BUILD_TESTS=ON \
  -DWILFRED_BUILD_BENCH=ON \
  -DWILFRED_ENABLE_SANITIZERS="$SANITIZERS" \
  "${EXTRA_CMAKE[@]}"

BUILD_ARGS=(--build "$BUILD_DIR" --config "$CONFIG")
if [[ -n "$JOBS" ]]; then
  BUILD_ARGS+=(--parallel "$JOBS")
else
  BUILD_ARGS+=(--parallel)
fi
cmake "${BUILD_ARGS[@]}"

if [[ "$RUN_TESTS" -eq 1 ]]; then
  ctest --test-dir "$BUILD_DIR" --output-on-failure -C "$CONFIG"
fi

echo
echo "Built:"
echo "  $BUILD_DIR/wilfred"
echo "  $BUILD_DIR/wilfred_tests"
echo "  $BUILD_DIR/wilfred_bench"
