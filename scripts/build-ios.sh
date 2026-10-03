#!/usr/bin/env bash
# Build the Wilfred iOS app (SwiftUI + C++ core, no NDK equivalent: the
# Xcode project compiles wilfred_core directly).
# Usage:
#   ./scripts/build-ios.sh [--simulator] [--device] [--release] [--debug] [--clean]
# Requires: macOS with Xcode 15+ (iOS 17 SDK). Simulator builds need no
# signing (CODE_SIGNING_ALLOWED=NO); device builds need a team:
#   WILFRED_APPLE_TEAM_ID=XXXXXXXXXX ./scripts/build-ios.sh --device
# Version overrides (release CI sets these from the tag):
#   WILFRED_VERSION_NAME=1.2.3 WILFRED_VERSION_CODE=1002003
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

DEST="simulator"
CONFIG="Release"
CLEAN=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --simulator) DEST="simulator"; shift ;;
    --device) DEST="device"; shift ;;
    --release) CONFIG="Release"; shift ;;
    --debug) CONFIG="Debug"; shift ;;
    --clean) CLEAN=1; shift ;;
    -h|--help)
      echo "Usage: $(basename "$0") [--simulator|--device] [--release|--debug] [--clean]"
      exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

if [[ "$(uname)" != "Darwin" ]]; then
  echo "iOS builds require macOS with Xcode." >&2
  exit 1
fi
if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "xcodebuild not found; install Xcode 15+." >&2
  exit 1
fi

# Regenerate the Xcode project so it can never drift from CMakeLists.txt
# (C++ file list) or ios/Wilfred (Swift sources).
python3 scripts/generate-ios-project.py

DERIVED="$ROOT/ios/build/DerivedData"
if [[ "$CLEAN" -eq 1 ]]; then
  rm -rf "$ROOT/ios/build"
fi

ARGS=(-project ios/Wilfred.xcodeproj
  -scheme Wilfred
  -configuration "$CONFIG"
  -derivedDataPath "$DERIVED")

if [[ -n "${WILFRED_VERSION_NAME:-}" ]]; then
  ARGS+=(MARKETING_VERSION="$WILFRED_VERSION_NAME")
fi
if [[ -n "${WILFRED_VERSION_CODE:-}" ]]; then
  ARGS+=(CURRENT_PROJECT_VERSION="$WILFRED_VERSION_CODE")
fi

if [[ "$DEST" == "simulator" ]]; then
  echo "Building for simulator ($CONFIG; unsigned)…"
  xcodebuild "${ARGS[@]}" \
    -destination 'generic/platform=iOS Simulator' \
    CODE_SIGNING_ALLOWED=NO build
else
  if [[ -z "${WILFRED_APPLE_TEAM_ID:-}" ]]; then
    echo "WILFRED_APPLE_TEAM_ID must be set for device builds." >&2
    exit 1
  fi
  echo "Building for device ($CONFIG; team $WILFRED_APPLE_TEAM_ID)…"
  xcodebuild "${ARGS[@]}" \
    -destination 'generic/platform=iOS' \
    DEVELOPMENT_TEAM="$WILFRED_APPLE_TEAM_ID" build
fi

echo
echo "App:"
find "$DERIVED/Build/Products" -maxdepth 2 -name '*.app' 2>/dev/null || true
