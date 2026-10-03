#!/usr/bin/env bash
# Build Wilfred on BSD (FreeBSD/OpenBSD/NetBSD/DragonFly, CMake + Clang/GCC).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

CONFIG="${WILFRED_CONFIG:-Release}"
BUILD_DIR="${WILFRED_BUILD_DIR:-build/bsd}"
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
  --build-dir DIR      CMake binary dir (default: build/bsd)
  --jobs N             Parallel compile jobs
  --tests              Run ctest after the build
  --sanitizers         ASan/UBSan (Debug-friendly)
  --deps               Install base packages (cmake, ninja, pkgconf)
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

case "$(uname -s)" in
  FreeBSD|OpenBSD|NetBSD|DragonFly) ;;
  *)
    echo "build-bsd.sh must run on FreeBSD, OpenBSD, NetBSD, or DragonFly." >&2
    exit 1
    ;;
esac

if [[ "$INSTALL_DEPS" -eq 1 ]]; then
  # Optional GUI pieces (X11 headers, WebKitGTK, gtk-layer-shell) are left
  # out on purpose: the build degrades gracefully without them (null
  # overlay, no global hotkey). Install them separately for the full
  # desktop experience, e.g. on FreeBSD:
  #   pkg install libX11 webkit2-gtk3 gtk-layer-shell
  if command -v pkg >/dev/null 2>&1; then
    sudo pkg install -y cmake ninja pkgconf 2>/dev/null || \
      pkg install -y cmake ninja pkgconf
  elif command -v pkg_add >/dev/null 2>&1; then
    echo "Run as root: pkg_add cmake ninja pkgconf" >&2
    exit 1
  elif command -v pkgin >/dev/null 2>&1; then
    sudo pkgin -y install cmake ninja pkgconf 2>/dev/null || \
      pkgin -y install cmake ninja pkgconf
  else
    echo "No supported package tool found (pkg, pkg_add, pkgin). Install cmake, a C++20 compiler, ninja, and pkgconf." >&2
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
