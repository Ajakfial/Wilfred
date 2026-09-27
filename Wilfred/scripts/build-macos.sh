#!/usr/bin/env bash
# Build Wilfred on macOS (CMake + Apple Clang / ObjC++).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CONFIG="${WILFRED_CONFIG:-Release}"
BUILD_DIR="${WILFRED_BUILD_DIR:-build/macos}"
JOBS="${WILFRED_JOBS:-}"
SANITIZERS=OFF
RUN_TESTS=0
INSTALL_DEPS=0
CLEAN=0
ARCH="${WILFRED_ARCH:-}"
GENERATOR=""
EXTRA_CMAKE=()

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

  --debug              Debug build
  --release            Release build (default)
  --build-dir DIR      CMake binary dir (default: build/macos)
  --jobs N             Parallel compile jobs
  --tests              Run ctest after the build
  --sanitizers         ASan/UBSan
  --arch ARCH          CMAKE_OSX_ARCHITECTURES (arm64, x86_64, or arm64;x86_64)
  --deps               Install cmake/ninja via Homebrew if missing
  --clean              Remove the build directory first
  --generator NAME     CMake generator (Ninja, Unix Makefiles, or Xcode)
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
    --arch) ARCH="$2"; shift 2 ;;
    --deps) INSTALL_DEPS=1; shift ;;
    --clean) CLEAN=1; shift ;;
    --generator) GENERATOR="$2"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    --) shift; EXTRA_CMAKE+=("$@"); break ;;
    *) EXTRA_CMAKE+=("$1"); shift ;;
  esac
done

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "build-macos.sh must run on macOS." >&2
  exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
  echo "Xcode Command Line Tools are required. Run: xcode-select --install" >&2
  exit 1
fi

if [[ "$INSTALL_DEPS" -eq 1 ]]; then
  if ! command -v brew >/dev/null 2>&1; then
    echo "Homebrew is required for --deps. See https://brew.sh" >&2
    exit 1
  fi
  brew install cmake ninja
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "cmake is required (3.16+). Try: brew install cmake" >&2
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

CMAKE_ARGS=(
  -S "$ROOT"
  -B "$BUILD_DIR"
  -G "$GENERATOR"
  -DCMAKE_BUILD_TYPE="$CONFIG"
  -DWILFRED_BUILD_TESTS=ON
  -DWILFRED_BUILD_BENCH=ON
  -DWILFRED_ENABLE_SANITIZERS="$SANITIZERS"
)
if [[ -n "$ARCH" ]]; then
  CMAKE_ARGS+=(-DCMAKE_OSX_ARCHITECTURES="$ARCH")
fi
if [[ ${#EXTRA_CMAKE[@]} -gt 0 ]]; then
  CMAKE_ARGS+=("${EXTRA_CMAKE[@]}")
fi

cmake "${CMAKE_ARGS[@]}"

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
